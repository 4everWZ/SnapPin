add_library(snappin_capture STATIC
  CaptureService.h
  CaptureServiceGdi.cpp
)

target_link_libraries(snappin_capture PUBLIC snappin_core
  d3d11 dxgi
)

if(SNAPPIN_ENABLE_WGC)
  target_link_libraries(snappin_capture PUBLIC windowsapp)
endif()

target_compile_definitions(snappin_capture PUBLIC
  $<$<BOOL:${SNAPPIN_ENABLE_WGC}>:SNAPPIN_ENABLE_WGC>
  $<$<BOOL:${SNAPPIN_ENABLE_DXGI_DUP}>:SNAPPIN_ENABLE_DXGI_DUP>
)

target_include_directories(snappin_capture PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
snappin_apply_warnings(snappin_capture)
