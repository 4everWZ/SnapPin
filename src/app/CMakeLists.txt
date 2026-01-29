add_executable(snappin_app
  AppMain.cpp
  SingleInstance.cpp
  SingleInstance.h
  TrayIcon.cpp
  TrayIcon.h
  ActionRegistry.cpp
  ActionRegistry.h
  ActionDispatcher.cpp
  ActionDispatcher.h
  ConfigService.cpp
  ConfigService.h
  KeybindingsService.cpp
  KeybindingsService.h
  ArtifactStore.cpp
  ArtifactStore.h
  StatsService.cpp
  StatsService.h
)

target_link_libraries(snappin_app PRIVATE
  snappin_core
  snappin_ui
  snappin_capture
  snappin_export
  advapi32
  shell32
  psapi
)

snappin_apply_warnings(snappin_app)

set_target_properties(snappin_app PROPERTIES
  WIN32_EXECUTABLE YES
  OUTPUT_NAME "snappin"
)
