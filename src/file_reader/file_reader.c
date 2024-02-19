/* Copyright 2020 Mats Kindahl
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

/*
 * This file introduce the quarternion type.
 *
 * It is based on the example un "User-Defined Types" at
 * https://www.postgresql.org/docs/10/xtypes.html
 */

#include "file_reader.h"

//#include <postgres.h>
#include <fmgr.h>


#include <libxml/chvalid.h>
#include <libxml/entities.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlsave.h>
#include <libxml/xmlversion.h>
#include <libxml/xmlwriter.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

/*
 * We used to check for xmlStructuredErrorContext via a configure test; but
 * that doesn't work on Windows, so instead use this grottier method of
 * testing the library version number.
 */


#include <lib/stringinfo.h>
#include <libpq/pqformat.h>
#include "mb/pg_wchar.h"

#include "executor/spi.h"
#include "executor/tablefunc.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datetime.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/xml.h"
#include "utils/json.h"
#include "utils/jsonb.h"


#include <string.h>
#include <funcapi.h>


PG_MODULE_MAGIC;

xmlChar* getValue(xmlXPathContextPtr xPathCtx, char* xPath);

PG_FUNCTION_INFO_V1(read_xml_file);

Datum read_xml_file(PG_FUNCTION_ARGS) {
    text *filename = PG_GETARG_TEXT_PP(0);
    text *xpathArray = PG_GETARG_TEXT_PP(1);

    char *p_filename[VARSIZE_ANY_EXHDR(filename)];
    snprintf(p_filename, VARSIZE_ANY_EXHDR(filename)+1, "%s", VARDATA_ANY(filename));

    char *p_xpathArray[VARSIZE_ANY_EXHDR(xpathArray)];
    snprintf(p_xpathArray, VARSIZE_ANY_EXHDR(xpathArray)+1, "%s", VARDATA_ANY(xpathArray));

    xmlDoc *doc = NULL;

    int charCount = 32;

    char*** words;
    FuncCallContext     *funcctx;
    int                  call_cntr;
    int                  max_calls;
    TupleDesc            tupdesc;
    AttInMetadata *attinmeta;

    MemoryContext oldcontext;
    if (SRF_IS_FIRSTCALL())
    {
        ArrayType* xpathes = PG_GETARG_ARRAYTYPE_P_COPY(2);
        Datum	   *key_datums;
        bool	   *key_nulls;
        int			elem_count;

        deconstruct_array_builtin(xpathes, TEXTOID, &key_datums, &key_nulls, &elem_count);

        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
            elog(ERROR, "return type must be a row type");

        attinmeta = TupleDescGetAttInMetadata(tupdesc);
        funcctx->attinmeta = attinmeta;
        funcctx->tuple_desc = tupdesc;

        doc = xmlReadFile(p_filename, NULL, 0);

        if (doc == NULL) {
            elog(NOTICE,"Could not parse the XML file");
        }

        xmlXPathContextPtr xPathCtx = xmlXPathNewContext(doc);
        xPathCtx->node = xmlDocGetRootElement(doc);

        xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((xmlChar*)p_xpathArray, xPathCtx);
        if(xpathObj == NULL){
            elog(NOTICE,"Failed to evaluate xpath\n");
        }

        xmlNodeSetPtr nodeset = xpathObj->nodesetval;
        funcctx->max_calls = xmlXPathNodeSetGetLength(nodeset);
        words = (char***)palloc(funcctx->max_calls * sizeof(char**));

        for (int i = 0; i < funcctx->max_calls; ++i) {
            words[i] = (char**)palloc(elem_count * sizeof(char*));
        }

        if(!xmlXPathNodeSetIsEmpty(nodeset)){
            printf("Found nodes using: \n");

            for(int i=0; i<xmlXPathNodeSetGetLength(nodeset); i++)
            {
                xmlNodePtr node = xmlXPathNodeSetItem(nodeset,i);
                xPathCtx->node = node;
                for(int j=0; j<elem_count; j++)
                {
                    char *xpathdata[VARSIZE_ANY_EXHDR(key_datums[j])];
                    snprintf(xpathdata, VARSIZE_ANY_EXHDR(key_datums[j])+1 , "%s", VARDATA_ANY(key_datums[j]));

                    char * data = (char *)getValue(xPathCtx, xpathdata);
                    if(data == NULL)
                    {
                        words[i][j] = NULL;

                    }
                    else
                    {
                        words[i][j] = (char *) palloc(charCount * sizeof(char));
                        words[i][j] = data;
                    }
                }
            }
            funcctx->user_fctx = words;
        }
        else
        {
            elog(NOTICE, "Failed to find nodes using: \n");
        }
        xmlFreeDoc(doc);
        xmlCleanupParser();
        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = SRF_PERCALL_SETUP();

    call_cntr = funcctx->call_cntr;
    max_calls = funcctx->max_calls;
    attinmeta = funcctx->attinmeta;
    words = funcctx->user_fctx;
    tupdesc = funcctx->tuple_desc;

    if (call_cntr < max_calls)
    {
        HeapTuple    tuple;
        Datum        result;

        ArrayType* xpathes = PG_GETARG_ARRAYTYPE_P_COPY(2);
        Datum	   key_datums;
        bool	   key_nulls;
        int			elem_count;
        deconstruct_array_builtin(xpathes, TEXTOID, &key_datums, &key_nulls, &elem_count);

        bool		nulls[elem_count];
        Datum       value_t[elem_count];

        for(int i = 0; i<elem_count;i++)
        {
            if(words[call_cntr][i] != NULL)
            {
                value_t[i] = cstring_to_text(words[call_cntr][i]);
                nulls[i] = false;
            }
            else
            {
                nulls[i] = true;
            }
        }
        tuple = heap_form_tuple(tupdesc, value_t, nulls);
        result = HeapTupleGetDatum(tuple);
        pfree(words[call_cntr]);
        SRF_RETURN_NEXT(funcctx, result);
    }
    else
    {

        SRF_RETURN_DONE(funcctx);
    }
}


xmlChar* getValue(xmlXPathContextPtr xPathCtx, char* xPath)
{
    xmlXPathObjectPtr xpathObj_uid = xmlXPathEvalExpression((xmlChar*)xPath, xPathCtx);
    if(xpathObj_uid == NULL){
        elog(NOTICE,"getValue Failed to evaluate xpath\n");
    }
    xmlNodeSetPtr nodeset_uid= xpathObj_uid->nodesetval;
    xmlNodePtr node_uid = xmlXPathNodeSetItem(nodeset_uid, 0);
    if(node_uid == 0x0)
    {
        return NULL;
    }
    return xmlNodeGetContent(node_uid);
}
