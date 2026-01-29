add_library(snappin_core STATIC
  Types.h
  ErrorCodes.h
  Action.h
  Artifact.h
  Stats.h
  CoreStub.cpp
)

target_include_directories(snappin_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
snappin_apply_warnings(snappin_core)

if(MSVC)
  target_compile_definitions(snappin_core PUBLIC UNICODE _UNICODE NOMINMAX)
endif()
