add_executable(snappin_tests
  core_tests.cpp
)

target_link_libraries(snappin_tests PRIVATE snappin_core)
snappin_apply_warnings(snappin_tests)

add_test(NAME snappin_tests COMMAND snappin_tests)
