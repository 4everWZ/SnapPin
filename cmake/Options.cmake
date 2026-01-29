# Build switches
option(SNAPPIN_ENABLE_WGC         "Enable Windows Graphics Capture backend" ON)
option(SNAPPIN_ENABLE_DXGI_DUP    "Enable DXGI Desktop Duplication backend" ON)

option(SNAPPIN_ENABLE_OCR         "Enable OCR module" OFF)
option(SNAPPIN_ENABLE_SCROLL      "Enable Scroll stitching module" OFF)
option(SNAPPIN_ENABLE_RECORD      "Enable Recording module" OFF)
option(SNAPPIN_ENABLE_UIA         "Enable UI Automation element detection" OFF)

option(SNAPPIN_ENABLE_DEBUG_PANEL "Enable debug panel (build-time)" OFF)
option(SNAPPIN_STRICT_WARNINGS    "Treat warnings as errors" ON)

option(SNAPPIN_BUILD_TESTS        "Build unit tests" ON)
