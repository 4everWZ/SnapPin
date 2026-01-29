add_library(snappin_ui STATIC
  UiStub.cpp
  OverlayWindow.cpp
  OverlayWindow.h
  ToolbarWindow.cpp
  ToolbarWindow.h
  SettingsWindow.cpp
  SettingsWindow.h
)

target_link_libraries(snappin_ui PUBLIC snappin_core
  d2d1 dwrite dxgi dwmapi user32 gdi32 msimg32
)

target_include_directories(snappin_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
snappin_apply_warnings(snappin_ui)
