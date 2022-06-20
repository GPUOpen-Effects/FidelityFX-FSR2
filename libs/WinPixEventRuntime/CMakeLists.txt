include(common)

add_library(winpixeventruntimelib SHARED IMPORTED GLOBAL)
set_property(TARGET winpixeventruntimelib PROPERTY IMPORTED_IMPLIB ${CMAKE_CURRENT_SOURCE_DIR}/WinPixEventRuntime.lib)

set(WINPIXEVENT_BIN
    "${CMAKE_CURRENT_SOURCE_DIR}/WinPixEventRuntime.dll"
)

copyTargetCommand("${WINPIXEVENT_BIN}" ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} copied_winpixevent_bin)
add_dependencies(winpixeventruntimelib copied_winpixevent_bin)
