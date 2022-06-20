include(${CMAKE_CURRENT_SOURCE_DIR}/../../common.cmake)

add_library(FSR2_Sample_Common INTERFACE)

set(config
    ${CMAKE_CURRENT_SOURCE_DIR}/../Common/FSR2_Sample.json
)

copyTargetCommand("${config}" ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} copied_common_config)
add_dependencies(FSR2_Sample_Common copied_common_config)
