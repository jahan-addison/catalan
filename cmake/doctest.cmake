# Test the application sources without the program entry point.
list(REMOVE_ITEM sources "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}/main.cc")

add_executable(Test_Suite ${sources} ${test_sources})

set_target_properties(Test_Suite PROPERTIES OUTPUT_NAME "test_suite")

target_include_directories(Test_Suite PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Test_Suite PRIVATE doctest::doctest
                                         sqlite_orm::sqlite_orm Crow::Crow)

target_compile_options(Test_Suite PRIVATE ${CATALAN_WARNINGS})
target_compile_definitions(
  Test_Suite PRIVATE ROOT_TEST_PATH=${CMAKE_CURRENT_SOURCE_DIR} DEBUG)

enable_testing()

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

if(CMAKE_CROSSCOMPILING)
  # When cross-compiling, do not attempt to run the binary on the host.
  add_test(NAME Test_Suite COMMAND Test_Suite)
else()
  doctest_discover_tests(Test_Suite)
endif()
