add_library(snappin_export STATIC
  ExportService.h
  ExportService.cpp
)

target_link_libraries(snappin_export PUBLIC snappin_core
  windowscodecs ole32 user32 shlwapi
)

target_include_directories(snappin_export PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
snappin_apply_warnings(snappin_export)
