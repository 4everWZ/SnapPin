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

find_program(POWERSHELL_EXECUTABLE NAMES powershell pwsh)
if(POWERSHELL_EXECUTABLE)
  add_test(
    NAME no_reference_terms
    COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/no_reference_terms.ps1"
            -RepoRoot "${CMAKE_SOURCE_DIR}"
  )
  add_test(
    NAME no_reference_terms_history
    COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/no_reference_terms.ps1"
            -RepoRoot "${CMAKE_SOURCE_DIR}" -AllHistory
  )
  add_test(
    NAME ui_capture_smoke
    COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/ui_capture_smoke.ps1"
            -AppPath "$<TARGET_FILE:snappin_app>"
  )
  set_tests_properties(ui_capture_smoke PROPERTIES RUN_SERIAL TRUE TIMEOUT 20)
endif()
