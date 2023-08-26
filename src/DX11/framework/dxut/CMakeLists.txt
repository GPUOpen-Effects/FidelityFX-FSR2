# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

cmake_minimum_required (VERSION 3.11)

set(DXUT_VERSION 11.26)

project (DXUT
  VERSION ${DXUT_VERSION}
  DESCRIPTION "DXUT for DirectX 11"
  HOMEPAGE_URL "http://go.microsoft.com/fwlink/?LinkId=320437"
  LANGUAGES CXX)

option(ENABLE_CODE_ANALYSIS "Use Static Code Analysis on build" OFF)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/CMake")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/CMake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/CMake")

#--- Core Library
set(CORE_LIBRARY_HEADERS
    Core/DDSTextureLoader.h
    Core/dxerr.h
    Core/DXUT.h
    Core/DXUTDevice11.h
    Core/DXUTmisc.h
    Core/ScreenGrab.h
    Core/WICTextureLoader.h
)

set(CORE_LIBRARY_SOURCES
    Core/DDSTextureLoader.cpp
    Core/dxerr.cpp
    Core/DXUT.cpp
    Core/DXUTDevice11.cpp
    Core/DXUTmisc.cpp
    Core/ScreenGrab.cpp
    Core/WICTextureLoader.cpp
)

add_library(${PROJECT_NAME} STATIC ${CORE_LIBRARY_SOURCES} ${CORE_LIBRARY_HEADERS})

#--- Optional Library
set(OPT_LIBRARY_HEADERS
    Optional/DXUTcamera.h
    Optional/DXUTgui.h
    Optional/DXUTguiIME.h
    Optional/DXUTLockFreePipe.h
    Optional/DXUTres.h
    Optional/DXUTsettingsdlg.h
    Optional/ImeUi.h
    Optional/SDKmesh.h
    Optional/SDKmisc.h
)

set(OPT_LIBRARY_SOURCES
    Optional/DXUTcamera.cpp
    Optional/DXUTgui.cpp
    Optional/DXUTguiIME.cpp
    Optional/DXUTres.cpp
    Optional/DXUTsettingsdlg.cpp
    Optional/ImeUi.cpp
    Optional/SDKmesh.cpp
    Optional/SDKmisc.cpp
)

add_library(${PROJECT_NAME}Opt STATIC ${OPT_LIBRARY_SOURCES} ${OPT_CORE_LIBRARY_HEADERS})

if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.16")
    target_precompile_headers(${PROJECT_NAME} PRIVATE Core/DXUT.h)
    target_precompile_headers(${PROJECT_NAME}Opt PRIVATE Core/DXUT.h)
endif()

source_group(DXUT REGULAR_EXPRESSION Core/*.*)
source_group(DXUTOpt REGULAR_EXPRESSION Optional/*.*)

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Core>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)

target_include_directories(${PROJECT_NAME}Opt PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Optional>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
  PRIVATE Core/)

if(MSVC)
    # Use max Warning Level
    string(REPLACE "/W3 " "/W4 " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    string(REPLACE "/W3 " "/W4 " CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
    string(REPLACE "/W3 " "/W4 " CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})

    # Library needs /EHsc (Enable C++ exceptions)
endif()

#--- Package
include(CMakePackageConfigHelpers)

string(TOLOWER ${PROJECT_NAME} PACKAGE_NAME)

write_basic_package_version_file(
  ${PROJECT_BINARY_DIR}/cmake/${PACKAGE_NAME}-config-version.cmake
  VERSION ${DXUT_VERSION}
  COMPATIBILITY AnyNewerVersion)

install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}-targets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(TARGETS ${PROJECT_NAME}Opt
  EXPORT ${PROJECT_NAME}Opt-targets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}-config.cmake.in
  ${PROJECT_BINARY_DIR}/cmake/${PACKAGE_NAME}-config.cmake
  INSTALL_DESTINATION cmake/})

install(EXPORT ${PROJECT_NAME}-targets
  FILE ${PROJECT_NAME}-targets.cmake
  NAMESPACE Microsoft::
  DESTINATION cmake/)

install(EXPORT ${PROJECT_NAME}Opt-targets
  FILE ${PROJECT_NAME}Opt-targets.cmake
  NAMESPACE Microsoft::
  DESTINATION cmake/)

install(FILES ${CORE_LIBRARY_HEADERS} ${OPT_LIBRARY_HEADERS}
    DESTINATION include/${PROJECT_NAME})

install(FILES
    ${PROJECT_BINARY_DIR}/cmake/${PACKAGE_NAME}-config.cmake
    ${PROJECT_BINARY_DIR}/cmake/${PACKAGE_NAME}-config-version.cmake
  DESTINATION cmake/)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /fp:fast)
    target_compile_options(${PROJECT_NAME}Opt PRIVATE /fp:fast)

   if(ENABLE_CODE_ANALYSIS)
      target_compile_options(${PROJECT_NAME} PRIVATE /analyze)
      target_compile_options(${PROJECT_NAME}Opt PRIVATE /analyze)
   endif()

    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.26)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zc:preprocessor /wd5104 /wd5105)
        target_compile_options(${PROJECT_NAME}Opt PRIVATE /Zc:preprocessor /wd5104 /wd5105)
    endif()

    if (${CMAKE_SIZEOF_VOID_P} EQUAL "4")
        target_compile_options(${PROJECT_NAME} PRIVATE /arch:SSE2)
        target_compile_options(${PROJECT_NAME}Opt PRIVATE /arch:SSE2)
    endif()
endif()

if ( CMAKE_CXX_COMPILER_ID MATCHES "MSVC" )
    target_compile_options(${PROJECT_NAME} PRIVATE /permissive- /JMC- /Zc:__cplusplus)
    target_compile_options(${PROJECT_NAME}Opt PRIVATE /permissive- /JMC- /Zc:__cplusplus)
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _UNICODE UNICODE)
    target_compile_definitions(${PROJECT_NAME}Opt PRIVATE _UNICODE UNICODE)

    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
    target_compile_definitions(${PROJECT_NAME}Opt PRIVATE _WIN32_WINNT=0x0601)
endif()
