add_executable(snappin_tests
  core_tests.cpp
  ../src/app/ActionRegistry.cpp
  ../src/app/TrayIcon.cpp
)

target_link_libraries(snappin_tests PRIVATE snappin_core snappin_ui)
target_include_directories(snappin_tests PRIVATE ../src/app)
target_link_libraries(snappin_tests PRIVATE shell32)
snappin_apply_warnings(snappin_tests)

add_test(NAME snappin_tests COMMAND snappin_tests)
