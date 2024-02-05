# CMake generated Testfile for 
# Source directory: /home/panchuk/Programming/C++/Postgresql/file_reader/src/file_reader
# Build directory: /home/panchuk/Programming/C++/Postgresql/file_reader/build/src/file_reader
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(file_reader "/opt/pgpro/std-16/lib/pgxs/src/test/regress/pg_regress" "--temp-instance=/home/panchuk/Programming/C++/Postgresql/file_reader/build/tmp_check" "--inputdir=/home/panchuk/Programming/C++/Postgresql/file_reader/src/file_reader" "--outputdir=/home/panchuk/Programming/C++/Postgresql/file_reader/build/src/file_reader" "--load-extension=file_reader" "basic")
set_tests_properties(file_reader PROPERTIES  _BACKTRACE_TRIPLES "/home/panchuk/Programming/C++/Postgresql/file_reader/cmake/FindPostgreSQL.cmake;271;add_test;/home/panchuk/Programming/C++/Postgresql/file_reader/src/file_reader/CMakeLists.txt;15;add_postgresql_extension;/home/panchuk/Programming/C++/Postgresql/file_reader/src/file_reader/CMakeLists.txt;0;")
