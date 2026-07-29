cmake_minimum_required(VERSION 3.26)

project(TVBPerf LANGUAGES CXX RC)

include(ExternalProject)
include(FetchContent)
include(CheckIPOSupported)

# =============================================================================
# Supported build environment
# =============================================================================

if(NOT WIN32)
    message(FATAL_ERROR "TVBPerf is a Windows-only Direct3D 12 project.")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "TVBPerf currently requires the MSVC toolchain.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "TVBPerf supports only 64-bit builds.")
endif()

# The repository's CMakeSettings.json uses one Ninja build tree per
# configuration. Keep that contract explicit because OpenUSD and oneTBB are
# installed into configuration-specific SDK directories.
if(CMAKE_CONFIGURATION_TYPES)
    message(FATAL_ERROR
        "TVBPerf requires a single-config generator. "
        "Use the x64-Debug or x64-Release Ninja configuration from CMakeSettings.json."
    )
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND
   NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR
        "Unsupported CMAKE_BUILD_TYPE='${CMAKE_BUILD_TYPE}'. "
        "TVBPerf supports Debug and Release."
    )
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Keep all in-tree libraries on the same CRT. Resolve the concrete value again
# below when configuring ExternalProject dependencies.
set(CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(TVBPERF_EXTERNAL_MSVC_RUNTIME "MultiThreadedDebugDLL")
    set(TVBPERF_TBB_LIBRARY_NAME "tbb12_debug.lib")
else()
    set(TVBPERF_EXTERNAL_MSVC_RUNTIME "MultiThreadedDLL")
    set(TVBPERF_TBB_LIBRARY_NAME "tbb12.lib")
endif()

# Avoid remote update checks after a dependency has been populated. Version
# changes in this file still cause the appropriate dependency to be refreshed.
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL
    "Do not contact remotes for already-populated FetchContent dependencies"
    FORCE
)

# Third-party libraries are linked statically except for the prebuilt DXC DLL.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static third-party libraries" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable third-party tests" FORCE)

# =============================================================================
# Project options and paths
# =============================================================================

option(TVBPERF_BUILD_OPENUSD
    "Build the OpenUSD SDK required by the current Jungle USD importer"
    ON
)
option(TVBPERF_BUILD_TOOLS
    "Build standalone Jungle/OpenUSD inspection tools"
    ON
)
option(TVBPERF_BUILD_TESTS
    "Build TVBPerf Jungle/OpenUSD regression tests"
    ON
)
option(TVBPERF_ENABLE_IPO
    "Enable interprocedural optimization for the Release renderer"
    ON
)

# The current master source tree always compiles SceneSourceFactory and
# JungleSceneSourceBuilder, so disabling OpenUSD would leave unresolved symbols.
# Keep the old option for cache compatibility, but reject an invalid build.
if(NOT TVBPERF_BUILD_OPENUSD)
    message(FATAL_ERROR
        "TVBPERF_BUILD_OPENUSD=OFF is not supported by the current source tree. "
        "SceneSourceFactory routes .usd/.usda files through the Jungle OpenUSD importer."
    )
endif()

set(TVBPERF_SRC_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(TVBPERF_INCLUDE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(TVBPERF_ASSETS_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(TVBPERF_SHADER_DIR   "${TVBPERF_ASSETS_DIR}/shaders")
set(TVBPERF_EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")

# Optional local source overrides. When left empty, an existing source tree in
# external/ is reused; otherwise CMake downloads the pinned revision.
set(TVBPERF_ASSIMP_SOURCE_DIR "" CACHE PATH
    "Optional local Assimp source directory"
)
set(TVBPERF_ONETBB_SOURCE_DIR "" CACHE PATH
    "Optional local oneTBB source directory"
)
set(TVBPERF_OPENUSD_SOURCE_DIR "" CACHE PATH
    "Optional local OpenUSD source directory"
)
mark_as_advanced(
    TVBPERF_ASSIMP_SOURCE_DIR
    TVBPERF_ONETBB_SOURCE_DIR
    TVBPERF_OPENUSD_SOURCE_DIR
)

function(tvbperf_resolve_local_source output_variable override_path fallback_path)
    set(_resolved "")

    if(NOT "${override_path}" STREQUAL "")
        get_filename_component(_resolved
            "${override_path}"
            ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
        )
        if(NOT EXISTS "${_resolved}/CMakeLists.txt")
            message(FATAL_ERROR
                "Local dependency source '${_resolved}' does not contain CMakeLists.txt."
            )
        endif()
    elseif(EXISTS "${fallback_path}/CMakeLists.txt")
        get_filename_component(_resolved "${fallback_path}" ABSOLUTE)
    endif()

    set(${output_variable} "${_resolved}" PARENT_SCOPE)
endfunction()

tvbperf_resolve_local_source(
    TVBPERF_ASSIMP_LOCAL_SOURCE
    "${TVBPERF_ASSIMP_SOURCE_DIR}"
    "${TVBPERF_EXTERNAL_DIR}/assimp"
)
tvbperf_resolve_local_source(
    TVBPERF_ONETBB_LOCAL_SOURCE
    "${TVBPERF_ONETBB_SOURCE_DIR}"
    "${TVBPERF_EXTERNAL_DIR}/oneTBB"
)
tvbperf_resolve_local_source(
    TVBPERF_OPENUSD_LOCAL_SOURCE
    "${TVBPERF_OPENUSD_SOURCE_DIR}"
    "${TVBPERF_EXTERNAL_DIR}/OpenUSD"
)

# =============================================================================
# Common project compile settings
# =============================================================================

add_library(TVBPerfProjectOptions INTERFACE)
add_library(TVBPerf::ProjectOptions ALIAS TVBPerfProjectOptions)

set_target_properties(TVBPerfProjectOptions PROPERTIES FOLDER "CMake")

target_compile_features(TVBPerfProjectOptions INTERFACE cxx_std_20)
target_compile_definitions(TVBPerfProjectOptions INTERFACE
    UNICODE
    _UNICODE
    NOMINMAX
    "$<$<CONFIG:Debug>:_DEBUG>"
    "$<$<CONFIG:Release>:NDEBUG>"
)
target_compile_options(TVBPerfProjectOptions INTERFACE
    /W3
    /sdl
    /permissive-
    /Zc:__cplusplus
    /utf-8
)

# =============================================================================
# DirectXTex
# =============================================================================

# Texture decoding only. GPU resources and uploads are managed by TVBPerf.
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_DX11 OFF CACHE BOOL "" FORCE)
set(BUILD_DX12 OFF CACHE BOOL "" FORCE)
set(BC_USE_OPENMP OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    directxtex
    GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
    GIT_TAG may2026
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(directxtex)

if(NOT TARGET DirectXTex)
    message(FATAL_ERROR "DirectXTex did not create the expected DirectXTex target.")
endif()
set_target_properties(DirectXTex PROPERTIES FOLDER "External/DirectXTex")

# =============================================================================
# fastgltf
# =============================================================================

set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_DOCS OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_GLTF_RS OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_ASSIMP OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_CPP_MODULES OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG v0.9.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fastgltf)

if(NOT TARGET fastgltf)
    message(FATAL_ERROR "fastgltf did not create the expected fastgltf target.")
endif()
set_target_properties(fastgltf PROPERTIES FOLDER "External/fastgltf")

# =============================================================================
# DirectX Shader Compiler (official prebuilt NuGet package)
# =============================================================================

FetchContent_Declare(
    dxc
    URL https://www.nuget.org/api/v2/package/Microsoft.Direct3D.DXC/1.8.2505.32
    URL_HASH SHA256=C6E82B70C14552F1DD58E4A79C93EEAB1567EEB0A9EE63A51564C410429BCE3E
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(dxc)

set(TVBPERF_DXC_ROOT "${dxc_SOURCE_DIR}/build/native")
set(TVBPERF_DXC_ARCH x64)
set(TVBPERF_DXCOMPILER_DLL
    "${TVBPERF_DXC_ROOT}/bin/${TVBPERF_DXC_ARCH}/dxcompiler.dll"
)
set(TVBPERF_DXIL_DLL
    "${TVBPERF_DXC_ROOT}/bin/${TVBPERF_DXC_ARCH}/dxil.dll"
)

foreach(_required_dxc_file IN ITEMS
    "${TVBPERF_DXC_ROOT}/include/dxcapi.h"
    "${TVBPERF_DXC_ROOT}/lib/${TVBPERF_DXC_ARCH}/dxcompiler.lib"
    "${TVBPERF_DXCOMPILER_DLL}"
    "${TVBPERF_DXIL_DLL}"
)
    if(NOT EXISTS "${_required_dxc_file}")
        message(FATAL_ERROR "DXC package file is missing: ${_required_dxc_file}")
    endif()
endforeach()

add_library(DXC::dxcompiler SHARED IMPORTED GLOBAL)
set_target_properties(DXC::dxcompiler PROPERTIES
    IMPORTED_IMPLIB
        "${TVBPERF_DXC_ROOT}/lib/${TVBPERF_DXC_ARCH}/dxcompiler.lib"
    IMPORTED_LOCATION
        "${TVBPERF_DXCOMPILER_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES
        "${TVBPERF_DXC_ROOT}/include"
)

# =============================================================================
# Assimp
# =============================================================================

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)

if(TVBPERF_ASSIMP_LOCAL_SOURCE)
    message(STATUS "Using local Assimp source: ${TVBPERF_ASSIMP_LOCAL_SOURCE}")
    add_subdirectory(
        "${TVBPERF_ASSIMP_LOCAL_SOURCE}"
        "${CMAKE_BINARY_DIR}/external/assimp-build"
        EXCLUDE_FROM_ALL
    )
else()
    message(STATUS "Fetching Assimp v5.4.3")
    FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.4.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(assimp)
endif()

if(NOT TARGET assimp)
    message(FATAL_ERROR "Assimp did not create the expected assimp target.")
endif()
set_target_properties(assimp PROPERTIES FOLDER "External/assimp")

if(TARGET zlibstatic)
    set_target_properties(zlibstatic PROPERTIES FOLDER "External/assimp")
endif()
if(TARGET UpdateAssimpLibsDebugSymbolsAndDLLs)
    set_target_properties(UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTIES
        FOLDER "External/assimp"
    )
endif()

# =============================================================================
# OpenUSD and its dedicated oneTBB SDK
# =============================================================================

set(TVBPERF_EXTERNAL_BUILD_ROOT "${CMAKE_BINARY_DIR}/external")
set(TVBPERF_ONETBB_PREFIX_DIR
    "${TVBPERF_EXTERNAL_BUILD_ROOT}/OpenUSD_oneTBB-prefix"
)
set(TVBPERF_ONETBB_BUILD_DIR
    "${TVBPERF_EXTERNAL_BUILD_ROOT}/oneTBB-build"
)
set(TVBPERF_ONETBB_INSTALL_DIR
    "${TVBPERF_EXTERNAL_BUILD_ROOT}/oneTBB-install"
)
set(TVBPERF_OPENUSD_PREFIX_DIR
    "${TVBPERF_EXTERNAL_BUILD_ROOT}/OpenUSD-prefix"
)
set(TVBPERF_OPENUSD_BUILD_DIR
    "${TVBPERF_EXTERNAL_BUILD_ROOT}/OpenUSD-build"
)
set(TVBPERF_OPENUSD_INSTALL_DIR
    "${TVBPERF_EXTERNAL_BUILD_ROOT}/OpenUSD-install"
)

set(TVBPERF_TBB_LIBRARY
    "${TVBPERF_ONETBB_INSTALL_DIR}/lib/${TVBPERF_TBB_LIBRARY_NAME}"
)
set(TVBPERF_OPENUSD_LIBRARY
    "${TVBPERF_OPENUSD_INSTALL_DIR}/lib/usd_m.lib"
)

# External projects use the same compiler, Ninja executable, configuration and
# CRT as the renderer. INSTALL_DIR is a token replaced by ExternalProject.
set(TVBPERF_EXTERNAL_COMMON_CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>"
    "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}"
    "-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_MSVC_RUNTIME_LIBRARY:STRING=${TVBPERF_EXTERNAL_MSVC_RUNTIME}"
    "-DCMAKE_POLICY_DEFAULT_CMP0091:STRING=NEW"
)
if(CMAKE_RC_COMPILER)
    list(APPEND TVBPERF_EXTERNAL_COMMON_CMAKE_ARGS
        "-DCMAKE_RC_COMPILER:FILEPATH=${CMAKE_RC_COMPILER}"
    )
endif()

if(TVBPERF_ONETBB_LOCAL_SOURCE)
    message(STATUS "Using local oneTBB source: ${TVBPERF_ONETBB_LOCAL_SOURCE}")
    set(TVBPERF_ONETBB_SOURCE_ARGS
        SOURCE_DIR "${TVBPERF_ONETBB_LOCAL_SOURCE}"
    )
else()
    set(TVBPERF_ONETBB_SOURCE_ARGS
        GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
        GIT_TAG v2021.9.0
        GIT_SHALLOW TRUE
        UPDATE_DISCONNECTED TRUE
    )
endif()

ExternalProject_Add(OpenUSD_oneTBB
    ${TVBPERF_ONETBB_SOURCE_ARGS}
    PREFIX "${TVBPERF_ONETBB_PREFIX_DIR}"
    BINARY_DIR "${TVBPERF_ONETBB_BUILD_DIR}"
    INSTALL_DIR "${TVBPERF_ONETBB_INSTALL_DIR}"

    # These libraries are produced by the install step. Declare only the
    # library for this build configuration; declaring both Debug and Release
    # makes Ninja rebuild oneTBB forever because one file can never exist.
    INSTALL_BYPRODUCTS "${TVBPERF_TBB_LIBRARY}"

    CMAKE_ARGS
        ${TVBPERF_EXTERNAL_COMMON_CMAKE_ARGS}
        "-DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5"
        "-DBUILD_SHARED_LIBS:BOOL=OFF"
        "-DTBB_TEST:BOOL=OFF"
        "-DTBB_EXAMPLES:BOOL=OFF"
        "-DTBB_STRICT:BOOL=OFF"
)

if(TVBPERF_OPENUSD_LOCAL_SOURCE)
    message(STATUS "Using local OpenUSD source: ${TVBPERF_OPENUSD_LOCAL_SOURCE}")
    set(TVBPERF_OPENUSD_SOURCE_ARGS
        SOURCE_DIR "${TVBPERF_OPENUSD_LOCAL_SOURCE}"
    )
else()
    set(TVBPERF_OPENUSD_SOURCE_ARGS
        GIT_REPOSITORY https://github.com/PixarAnimationStudios/OpenUSD.git
        GIT_TAG v26.05
        GIT_SHALLOW TRUE
        UPDATE_DISCONNECTED TRUE
    )
endif()

ExternalProject_Add(OpenUSD
    ${TVBPERF_OPENUSD_SOURCE_ARGS}
    DEPENDS OpenUSD_oneTBB
    PREFIX "${TVBPERF_OPENUSD_PREFIX_DIR}"
    BINARY_DIR "${TVBPERF_OPENUSD_BUILD_DIR}"
    INSTALL_DIR "${TVBPERF_OPENUSD_INSTALL_DIR}"

    # usd_m.lib is installed by `cmake --install`, not generated directly by
    # the ExternalProject build step.
    INSTALL_BYPRODUCTS "${TVBPERF_OPENUSD_LIBRARY}"

    CMAKE_ARGS
        ${TVBPERF_EXTERNAL_COMMON_CMAKE_ARGS}
        "-DTBB_ROOT_DIR:PATH=${TVBPERF_ONETBB_INSTALL_DIR}"
        "-DTBB_DIR:PATH=${TVBPERF_ONETBB_INSTALL_DIR}/lib/cmake/TBB"
        "-DCMAKE_PREFIX_PATH:PATH=${TVBPERF_ONETBB_INSTALL_DIR}"
        "-DBUILD_SHARED_LIBS:BOOL=OFF"
        "-DPXR_BUILD_MONOLITHIC:BOOL=ON"
        "-DPXR_BUILD_TESTS:BOOL=OFF"
        "-DPXR_BUILD_EXAMPLES:BOOL=OFF"
        "-DPXR_BUILD_TUTORIALS:BOOL=OFF"
        "-DPXR_BUILD_USD_TOOLS:BOOL=OFF"
        "-DPXR_BUILD_IMAGING:BOOL=OFF"
        "-DPXR_BUILD_USD_IMAGING:BOOL=OFF"
        "-DPXR_BUILD_USD_VALIDATION:BOOL=OFF"
        "-DPXR_BUILD_EXEC:BOOL=OFF"
        "-DPXR_BUILD_USDVIEW:BOOL=OFF"
        "-DPXR_ENABLE_PYTHON_SUPPORT:BOOL=OFF"
        "-DPXR_ENABLE_GL_SUPPORT:BOOL=OFF"
        "-DPXR_ENABLE_PRECOMPILED_HEADERS:BOOL=OFF"
)

set_target_properties(OpenUSD_oneTBB OpenUSD PROPERTIES
    FOLDER "External/OpenUSD"
)

# CMake validates imported include directories during generation. The actual
# headers are populated by the ExternalProject install steps before compilation.
file(MAKE_DIRECTORY
    "${TVBPERF_OPENUSD_INSTALL_DIR}/include"
    "${TVBPERF_ONETBB_INSTALL_DIR}/include"
)

add_library(TVBPerfOpenUSD STATIC IMPORTED GLOBAL)
add_library(TVBPerf::OpenUSD ALIAS TVBPerfOpenUSD)
set_target_properties(TVBPerfOpenUSD PROPERTIES
    IMPORTED_LOCATION "${TVBPERF_OPENUSD_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES
        "${TVBPERF_OPENUSD_INSTALL_DIR}/include;${TVBPERF_ONETBB_INSTALL_DIR}/include"
)
target_compile_definitions(TVBPerfOpenUSD INTERFACE
    PXR_STATIC
    NOMINMAX
)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(TVBPerfOpenUSD INTERFACE TBB_USE_DEBUG)
endif()
target_link_libraries(TVBPerfOpenUSD INTERFACE
    "${TVBPERF_TBB_LIBRARY}"
    Shlwapi
    Dbghelp
    Ws2_32
)
target_link_options(TVBPerfOpenUSD INTERFACE
    "/WHOLEARCHIVE:${TVBPERF_OPENUSD_LIBRARY}"
)

# =============================================================================
# Source discovery
# =============================================================================

file(GLOB_RECURSE TVBPERF_CPP_FILES
    CONFIGURE_DEPENDS
    "${TVBPERF_SRC_DIR}/*.cpp"
)
file(GLOB_RECURSE TVBPERF_HEADER_FILES
    CONFIGURE_DEPENDS
    "${TVBPERF_INCLUDE_DIR}/*.h"
    "${TVBPERF_INCLUDE_DIR}/*.hpp"
)
file(GLOB_RECURSE TVBPERF_RC_FILES
    CONFIGURE_DEPENDS
    "${TVBPERF_SRC_DIR}/*.rc"
)
file(GLOB_RECURSE TVBPERF_SHADER_FILES
    CONFIGURE_DEPENDS
    "${TVBPERF_SHADER_DIR}/*.hlsl"
    "${TVBPERF_SHADER_DIR}/*.hlsli"
    "${TVBPERF_SHADER_DIR}/*.fx"
    "${TVBPERF_SHADER_DIR}/*.vs"
    "${TVBPERF_SHADER_DIR}/*.ps"
    "${TVBPERF_SHADER_DIR}/*.cs"
)

foreach(_source_list IN ITEMS
    TVBPERF_CPP_FILES
    TVBPERF_HEADER_FILES
    TVBPERF_RC_FILES
)
    list(FILTER ${_source_list} EXCLUDE REGEX "[/\\\\]trashes[/\\\\]")
    list(SORT ${_source_list})
endforeach()
list(SORT TVBPERF_SHADER_FILES)

# These implementations belong to dedicated OpenUSD targets and must not also
# be compiled directly into TVBPerf.
set(TVBPERF_SCENE_RAW_JUNGLE_CPP
    "${TVBPERF_SRC_DIR}/scene/raw/SceneRawJungle.cpp"
)
set(TVBPERF_SCENE_RAW_JUNGLE_TO_SOURCE_CPP
    "${TVBPERF_SRC_DIR}/scene/builder/source/SceneRawJungleToSource.cpp"
)
list(REMOVE_ITEM TVBPERF_CPP_FILES
    "${TVBPERF_SCENE_RAW_JUNGLE_CPP}"
    "${TVBPERF_SCENE_RAW_JUNGLE_TO_SOURCE_CPP}"
)

# =============================================================================
# OpenUSD scene targets
# =============================================================================

add_library(SceneRawJungle STATIC
    "${TVBPERF_SCENE_RAW_JUNGLE_CPP}"
    "${TVBPERF_INCLUDE_DIR}/scene/raw/SceneRawJungle.h"
    "${TVBPERF_INCLUDE_DIR}/scene/raw/SceneRawJungleUsdAccess.h"
)
target_link_libraries(SceneRawJungle
    PRIVATE
        TVBPerf::ProjectOptions
        TVBPerf::OpenUSD
)
target_include_directories(SceneRawJungle PUBLIC "${TVBPERF_INCLUDE_DIR}")
target_compile_definitions(SceneRawJungle PRIVATE
    "TVBPERF_OPENUSD_PLUGIN_ROOT=\"${TVBPERF_OPENUSD_INSTALL_DIR}/lib/usd\""
    "TVBPERF_OPENUSD_SHADER_PLUGIN_ROOT=\"${TVBPERF_OPENUSD_INSTALL_DIR}/plugin/usd\""
)
add_dependencies(SceneRawJungle OpenUSD)
set_target_properties(SceneRawJungle PROPERTIES FOLDER "TVBPerf/OpenUSD")

# The standalone conversion tools do not link the renderer executable, so keep
# semantic validation and bounds implementation self-contained.
add_library(SceneRawJungleToSource STATIC
    "${TVBPERF_SCENE_RAW_JUNGLE_TO_SOURCE_CPP}"
    "${TVBPERF_INCLUDE_DIR}/scene/builder/source/SceneRawJungleToSource.h"
    "${TVBPERF_SRC_DIR}/math/AABB.cpp"
    "${TVBPERF_SRC_DIR}/scene/builder/source/SceneSourceSemanticValidator.cpp"
    "${TVBPERF_SRC_DIR}/util/Logger.cpp"
)
target_link_libraries(SceneRawJungleToSource
    PUBLIC
        SceneRawJungle
    PRIVATE
        TVBPerf::ProjectOptions
        TVBPerf::OpenUSD
)
target_include_directories(SceneRawJungleToSource PUBLIC "${TVBPERF_INCLUDE_DIR}")
add_dependencies(SceneRawJungleToSource OpenUSD)
set_target_properties(SceneRawJungleToSource PROPERTIES FOLDER "TVBPerf/OpenUSD")

if(TVBPERF_BUILD_TOOLS)
    add_executable(SceneRawJungleInspect
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/SceneRawJungleInspect.cpp"
    )
    target_link_libraries(SceneRawJungleInspect PRIVATE
        TVBPerf::ProjectOptions
        SceneRawJungle
    )
    set_target_properties(SceneRawJungleInspect PROPERTIES FOLDER "TVBPerf/Tools")

    add_executable(SceneRawJungleToSourceInspect
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/SceneRawJungleToSourceInspect.cpp"
    )
    target_link_libraries(SceneRawJungleToSourceInspect PRIVATE
        TVBPerf::ProjectOptions
        SceneRawJungleToSource
    )
    set_target_properties(SceneRawJungleToSourceInspect PROPERTIES
        FOLDER "TVBPerf/Tools"
    )
endif()

if(TVBPERF_BUILD_TESTS)
    enable_testing()

    add_executable(SceneRawJungleToSourceTest
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/SceneRawJungleToSourceTest.cpp"
    )
    target_link_libraries(SceneRawJungleToSourceTest PRIVATE
        TVBPerf::ProjectOptions
        SceneRawJungleToSource
    )
    target_compile_definitions(SceneRawJungleToSourceTest PRIVATE
        "SCENE_RAW_JUNGLE_SOURCE_FIXTURE=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/assets/scene_raw_jungle_to_source_fixture.usda\""
    )
    set_target_properties(SceneRawJungleToSourceTest PROPERTIES
        FOLDER "TVBPerf/Tests"
    )
    add_test(
        NAME SceneRawJungleToSourceTest
        COMMAND "$<TARGET_FILE:SceneRawJungleToSourceTest>"
    )

    add_executable(JungleSceneCPUBuilderTest
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/JungleSceneCPUBuilderTest.cpp"
        "${TVBPERF_SRC_DIR}/scene/builder/source/JungleSceneSourceBuilder.cpp"
        "${TVBPERF_SRC_DIR}/scene/builder/cpu/JungleSceneCPUBuilder.cpp"
        "${TVBPERF_SRC_DIR}/scene/builder/cpu/JungleSceneCPUDrawStreamBuilder.cpp"
        "${TVBPERF_SRC_DIR}/scene/builder/cpu/SceneCPUBuilder.cpp"
        "${TVBPERF_SRC_DIR}/scene/builder/cpu/SceneCPUValidator.cpp"
        "${TVBPERF_SRC_DIR}/scene/builder/source/SceneSourceDataValidator.cpp"
        "${TVBPERF_SRC_DIR}/scene/builder/source/SceneSourceGeometryValidator.cpp"
        "${TVBPERF_SRC_DIR}/scene/builder/source/SceneSourceHierarchyValidator.cpp"
    )
    target_link_libraries(JungleSceneCPUBuilderTest PRIVATE
        TVBPerf::ProjectOptions
        SceneRawJungleToSource
    )
    target_include_directories(JungleSceneCPUBuilderTest PRIVATE
        "${TVBPERF_INCLUDE_DIR}"
    )
    target_compile_definitions(JungleSceneCPUBuilderTest PRIVATE
        "JUNGLE_SCENE_CPU_SOURCE_FIXTURE=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/assets/scene_raw_jungle_to_source_fixture.usda\""
    )
    set_target_properties(JungleSceneCPUBuilderTest PROPERTIES
        FOLDER "TVBPerf/Tests"
    )
    add_test(
        NAME JungleSceneCPUBuilderTest
        COMMAND "$<TARGET_FILE:JungleSceneCPUBuilderTest>"
    )
endif()

# =============================================================================
# Renderer executable
# =============================================================================

add_executable(TVBPerf WIN32
    ${TVBPERF_CPP_FILES}
    ${TVBPERF_HEADER_FILES}
    ${TVBPERF_RC_FILES}
    ${TVBPERF_SHADER_FILES}
)

target_link_libraries(TVBPerf PRIVATE
    TVBPerf::ProjectOptions
    SceneRawJungleToSource
    assimp
    DirectXTex
    fastgltf::fastgltf
    DXC::dxcompiler
    d3d12
    dxgi
    d3dcompiler
    user32
    gdi32
    kernel32
    ole32
    windowscodecs
    Shell32
)

target_include_directories(TVBPerf PRIVATE
    "${TVBPERF_INCLUDE_DIR}"
    "${TVBPERF_SHADER_DIR}"
)

# CMake already supplies the standard /Od,/Zi and /O2 configuration flags.
# Keep only the additional release tuning used by the benchmark.
target_compile_options(TVBPerf PRIVATE
    "$<$<CONFIG:Release>:/Oi>"
    "$<$<CONFIG:Release>:/Ot>"
    "$<$<CONFIG:Release>:/Gy>"
)

target_link_options(TVBPerf PRIVATE
    /DEBUG
    "$<$<CONFIG:Debug>:/INCREMENTAL>"
    "$<$<CONFIG:Release>:/INCREMENTAL:NO>"
    "$<$<CONFIG:Release>:/OPT:REF>"
    "$<$<CONFIG:Release>:/OPT:ICF>"
)

if(TVBPERF_ENABLE_IPO)
    check_ipo_supported(RESULT TVBPERF_IPO_SUPPORTED OUTPUT TVBPERF_IPO_ERROR)
    if(TVBPERF_IPO_SUPPORTED)
        set_property(TARGET TVBPerf PROPERTY
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        )
    else()
        message(WARNING "Release IPO is unavailable: ${TVBPERF_IPO_ERROR}")
    endif()
endif()

set(TVBPERF_RUNTIME_OUTPUT_DIR
    "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}"
)
set_target_properties(TVBPerf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TVBPERF_RUNTIME_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY "${TVBPERF_RUNTIME_OUTPUT_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${TVBPERF_RUNTIME_OUTPUT_DIR}"
    FOLDER "TVBPerf"
)

# HLSL is compiled at runtime by DXC. Keep shader files visible in the IDE but
# prevent Visual Studio from assigning a build tool to them.
if(TVBPERF_SHADER_FILES)
    set_source_files_properties(${TVBPERF_SHADER_FILES} PROPERTIES
        VS_TOOL_OVERRIDE "None"
        HEADER_FILE_ONLY TRUE
    )
endif()

# =============================================================================
# Incremental runtime-file deployment
# =============================================================================

# The previous ALL custom target copied the complete shader directory on every
# build. These stamp commands run only when a source shader, DXC binary, or the
# generated shader manifest changes. BYPRODUCTS also lets Ninja restore a
# deleted runtime file without rebuilding unrelated C++ targets.
set(TVBPERF_RUNTIME_STAMP_DIR "${CMAKE_BINARY_DIR}/runtime-stamps")
file(MAKE_DIRECTORY "${TVBPERF_RUNTIME_STAMP_DIR}")

set(TVBPERF_RUNTIME_DEPLOY_STAMPS "")

if(TVBPERF_SHADER_FILES)
    set(TVBPERF_SHADER_OUTPUT_DIR
        "${TVBPERF_RUNTIME_OUTPUT_DIR}/assets/shaders"
    )
    set(TVBPERF_SHADER_MANIFEST
        "${TVBPERF_RUNTIME_STAMP_DIR}/shader-manifest.txt"
    )
    set(TVBPERF_SHADER_STAMP
        "${TVBPERF_RUNTIME_STAMP_DIR}/shaders.stamp"
    )
    set(TVBPERF_SHADER_OUTPUT_FILES "")
    set(_tvbperf_shader_manifest_content "")

    foreach(_shader IN LISTS TVBPERF_SHADER_FILES)
        file(RELATIVE_PATH _shader_relative
            "${TVBPERF_SHADER_DIR}"
            "${_shader}"
        )
        list(APPEND TVBPERF_SHADER_OUTPUT_FILES
            "${TVBPERF_SHADER_OUTPUT_DIR}/${_shader_relative}"
        )
        string(APPEND _tvbperf_shader_manifest_content
            "${_shader_relative}\n"
        )
    endforeach()

    # file(CONFIGURE) preserves the timestamp when the manifest content did not
    # change, so a normal CMake regeneration does not force another copy.
    file(CONFIGURE
        OUTPUT "${TVBPERF_SHADER_MANIFEST}"
        CONTENT "${_tvbperf_shader_manifest_content}"
        @ONLY
    )

    add_custom_command(
        OUTPUT "${TVBPERF_SHADER_STAMP}"
        BYPRODUCTS ${TVBPERF_SHADER_OUTPUT_FILES}
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${TVBPERF_SHADER_OUTPUT_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${TVBPERF_SHADER_OUTPUT_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${TVBPERF_SHADER_DIR}"
            "${TVBPERF_SHADER_OUTPUT_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${TVBPERF_SHADER_STAMP}"
        DEPENDS
            ${TVBPERF_SHADER_FILES}
            "${TVBPERF_SHADER_MANIFEST}"
        COMMENT "Updating runtime shaders"
        VERBATIM
    )
    list(APPEND TVBPERF_RUNTIME_DEPLOY_STAMPS "${TVBPERF_SHADER_STAMP}")
endif()

set(TVBPERF_DXC_STAMP "${TVBPERF_RUNTIME_STAMP_DIR}/dxc-runtime.stamp")
set(TVBPERF_DEPLOYED_DXCOMPILER
    "${TVBPERF_RUNTIME_OUTPUT_DIR}/dxcompiler.dll"
)
set(TVBPERF_DEPLOYED_DXIL
    "${TVBPERF_RUNTIME_OUTPUT_DIR}/dxil.dll"
)

add_custom_command(
    OUTPUT "${TVBPERF_DXC_STAMP}"
    BYPRODUCTS
        "${TVBPERF_DEPLOYED_DXCOMPILER}"
        "${TVBPERF_DEPLOYED_DXIL}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${TVBPERF_RUNTIME_OUTPUT_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${TVBPERF_DXCOMPILER_DLL}"
        "${TVBPERF_DEPLOYED_DXCOMPILER}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${TVBPERF_DXIL_DLL}"
        "${TVBPERF_DEPLOYED_DXIL}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${TVBPERF_DXC_STAMP}"
    DEPENDS
        "${TVBPERF_DXCOMPILER_DLL}"
        "${TVBPERF_DXIL_DLL}"
    COMMENT "Updating DXC runtime DLLs"
    VERBATIM
)
list(APPEND TVBPERF_RUNTIME_DEPLOY_STAMPS "${TVBPERF_DXC_STAMP}")

add_custom_target(TVBPerfRuntimeFiles
    DEPENDS ${TVBPERF_RUNTIME_DEPLOY_STAMPS}
)
add_dependencies(TVBPerf TVBPerfRuntimeFiles)
set_target_properties(TVBPerfRuntimeFiles PROPERTIES FOLDER "TVBPerf")

# =============================================================================
# IDE source groups
# =============================================================================

source_group(TREE "${TVBPERF_SRC_DIR}"
    PREFIX "Source"
    FILES ${TVBPERF_CPP_FILES} ${TVBPERF_RC_FILES}
)
source_group(TREE "${TVBPERF_INCLUDE_DIR}"
    PREFIX "Include"
    FILES ${TVBPERF_HEADER_FILES}
)
if(TVBPERF_SHADER_FILES)
    source_group(TREE "${TVBPERF_SHADER_DIR}"
        PREFIX "Shader Files"
        FILES ${TVBPERF_SHADER_FILES}
    )
endif()

message(STATUS "TVBPerf configuration: ${CMAKE_BUILD_TYPE}")
message(STATUS "TVBPerf runtime directory: ${TVBPERF_RUNTIME_OUTPUT_DIR}")
message(STATUS "OpenUSD library: ${TVBPERF_OPENUSD_LIBRARY}")
message(STATUS "oneTBB library: ${TVBPERF_TBB_LIBRARY}")
