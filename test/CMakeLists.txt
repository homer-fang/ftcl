cmake_minimum_required(VERSION 3.15)
# cmake --build .
# make -j16
# ctest -V
# Test executable for Tokenizer
add_executable(test_tokenizer test_tokenizer.cpp)

# Link against the ftcl library
target_link_libraries(test_tokenizer PRIVATE ftcl)

# Add test
add_test(NAME TokenizerTest COMMAND test_tokenizer)

# Test executable for EvalPtr
add_executable(test_eval_ptr test_eval_ptr.cpp)

# Link against the ftcl library
target_link_libraries(test_eval_ptr PRIVATE ftcl)

# Add test
add_test(NAME EvalPtrTest COMMAND test_eval_ptr)

# Smoke test: include and compile the full header surface.
add_executable(test_all_headers test_all_headers.cpp)
target_link_libraries(test_all_headers PRIVATE ftcl)
add_test(NAME AllHeadersTest COMMAND test_all_headers)

# Parser/substitution regression tests (P0-1)
add_executable(test_parser_subst test_parser_subst.cpp)
target_link_libraries(test_parser_subst PRIVATE ftcl)
add_test(NAME ParserSubstTest COMMAND test_parser_subst)

# Return/catch regression tests (P0-2)
add_executable(test_return_catch test_return_catch.cpp)
target_link_libraries(test_return_catch PRIVATE ftcl)
add_test(NAME ReturnCatchTest COMMAND test_return_catch)

# Procedure/scope regression tests (P0-3)
add_executable(test_proc_scope test_proc_scope.cpp)
target_link_libraries(test_proc_scope PRIVATE ftcl)
add_test(NAME ProcScopeTest COMMAND test_proc_scope)

# List regression tests (P0-4.1)
add_executable(test_list_semantics test_list_semantics.cpp)
target_link_libraries(test_list_semantics PRIVATE ftcl)
add_test(NAME ListSemanticsTest COMMAND test_list_semantics)

# Dict regression tests (P0-4.2)
add_executable(test_dict_semantics test_dict_semantics.cpp)
target_link_libraries(test_dict_semantics PRIVATE ftcl)
add_test(NAME DictSemanticsTest COMMAND test_dict_semantics)

# String regression tests (P0-4.3)
add_executable(test_string_semantics test_string_semantics.cpp)
target_link_libraries(test_string_semantics PRIVATE ftcl)
add_test(NAME StringSemanticsTest COMMAND test_string_semantics)

# Array regression tests (P0-4.4)
add_executable(test_array_semantics test_array_semantics.cpp)
target_link_libraries(test_array_semantics PRIVATE ftcl)
add_test(NAME ArraySemanticsTest COMMAND test_array_semantics)

# Info regression tests (P1-1)
add_executable(test_info_semantics test_info_semantics.cpp)
target_link_libraries(test_info_semantics PRIVATE ftcl)
add_test(NAME InfoSemanticsTest COMMAND test_info_semantics)

# Source regression tests (P1-2)
add_executable(test_source_semantics test_source_semantics.cpp)
target_link_libraries(test_source_semantics PRIVATE ftcl)
add_test(NAME SourceSemanticsTest COMMAND test_source_semantics)

# Time regression tests (P1-3)
add_executable(test_time_semantics test_time_semantics.cpp)
target_link_libraries(test_time_semantics PRIVATE ftcl)
add_test(NAME TimeSemanticsTest COMMAND test_time_semantics)

# Test harness regression tests (P1-4)
add_executable(test_test_harness_semantics test_test_harness_semantics.cpp)
target_link_libraries(test_test_harness_semantics PRIVATE ftcl)
add_test(NAME TestHarnessSemanticsTest COMMAND test_test_harness_semantics)

# Expr regression tests (loop-condition variable resolution)
add_executable(test_expr_semantics test_expr_semantics.cpp)
target_link_libraries(test_expr_semantics PRIVATE ftcl)
add_test(NAME ExprSemanticsTest COMMAND test_expr_semantics)

# Run original ftcl Tcl test suite via ftcl test_harness.
add_executable(test_ftcl_suite test_ftcl_suite.cpp)
target_link_libraries(test_ftcl_suite PRIVATE ftcl)
if(EXISTS "${CMAKE_SOURCE_DIR}/test/tests/all.tcl")
    add_test(
        NAME ftclSuiteTest
        COMMAND test_ftcl_suite ${CMAKE_SOURCE_DIR}/test/tests/all.tcl
    )
endif()

# ftcl Tcl subset that currently passes, used for ongoing regression.
add_executable(test_ftcl_subset test_ftcl_subset.cpp)
target_link_libraries(test_ftcl_subset PRIVATE ftcl)
if(EXISTS "${CMAKE_SOURCE_DIR}/test/tests")
    add_test(
        NAME ftclSubsetTest
        COMMAND test_ftcl_subset ${CMAKE_SOURCE_DIR}/test/tests
    )
endif()
# cd /mnt/d/ftcl/ftcl
# cmake --build build -j4
# cd build
# ctest -N
# ctest --output-on-failure