cmake_minimum_required(VERSION 3.26)

project(TVBPerf LANGUAGES CXX RC)

include(ExternalProject)
include(FetchContent)

# Visual Studio 멀티 구성 generator용 구성 명시
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
endif()

# C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Visual Studio 폴더 정리용
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 경로
set(TVBPERF_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(TVBPERF_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(TVBPERF_ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
set(TVBPERF_SHADER_DIR ${TVBPERF_ASSETS_DIR}/shaders)
set(TVBPERF_EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)
set(TVBPERF_ASSIMP_DIR ${TVBPERF_EXTERNAL_DIR}/assimp)
set(TVBPERF_OPENUSD_DIR ${TVBPERF_EXTERNAL_DIR}/OpenUSD)
set(TVBPERF_ONE_TBB_DIR ${TVBPERF_EXTERNAL_DIR}/oneTBB)

# static third-party libraries
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# ------------------------------------------------------------
# DirectXTex
# ------------------------------------------------------------
# Texture decoding only. GPU resources and uploads are managed by TVBPerf.
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_DX11 OFF CACHE BOOL "" FORCE)
set(BUILD_DX12 OFF CACHE BOOL "" FORCE)
set(BC_USE_OPENMP OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    directxtex
    GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
    GIT_TAG may2026
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(directxtex)

set_target_properties(DirectXTex PROPERTIES FOLDER "External/DirectXTex")

# ------------------------------------------------------------
# fastgltf
# ------------------------------------------------------------
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

set_target_properties(fastgltf PROPERTIES FOLDER "External/fastgltf")

# ------------------------------------------------------------
# DirectX Shader Compiler
# ------------------------------------------------------------
# Use the official binary package. Building DXC from source would pull in
# LLVM and is far heavier than this runtime shader-compilation use case.
FetchContent_Declare(
    dxc
    URL https://www.nuget.org/api/v2/package/Microsoft.Direct3D.DXC/1.8.2505.32
    URL_HASH SHA256=C6E82B70C14552F1DD58E4A79C93EEAB1567EEB0A9EE63A51564C410429BCE3E
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(dxc)

set(TVBPERF_DXC_ROOT ${dxc_SOURCE_DIR}/build/native)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TVBPERF_DXC_ARCH x64)
else()
    set(TVBPERF_DXC_ARCH x86)
endif()

add_library(DXC::dxcompiler SHARED IMPORTED GLOBAL)
set_target_properties(DXC::dxcompiler PROPERTIES
    IMPORTED_IMPLIB
        "${TVBPERF_DXC_ROOT}/lib/${TVBPERF_DXC_ARCH}/dxcompiler.lib"
    IMPORTED_LOCATION
        "${TVBPERF_DXC_ROOT}/bin/${TVBPERF_DXC_ARCH}/dxcompiler.dll"
    INTERFACE_INCLUDE_DIRECTORIES
        "${TVBPERF_DXC_ROOT}/include"
)

# ------------------------------------------------------------
# Assimp
# ------------------------------------------------------------
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)

# Exporter 필요 없으면 OFF
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)

add_subdirectory(${TVBPERF_ASSIMP_DIR})

set_target_properties(assimp PROPERTIES FOLDER "External/assimp")

if(TARGET zlibstatic)
    set_target_properties(zlibstatic PROPERTIES FOLDER "External/assimp")
endif()

if(TARGET UpdateAssimpLibsDebugSymbolsAndDLLs)
    set_target_properties(UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTIES FOLDER "External/assimp")
endif()

# ------------------------------------------------------------
# OpenUSD
# ------------------------------------------------------------
# OpenUSD is prepared as a standalone installed SDK for a future USD scene
# importer. It is intentionally not linked to TVBPerf until that importer exists.
option(TVBPERF_BUILD_OPENUSD "Build the OpenUSD core SDK for future USD scene import" ON)

if(TVBPERF_BUILD_OPENUSD)
    set(TVBPERF_OPENUSD_BUILD_DIR ${CMAKE_BINARY_DIR}/external/OpenUSD-build)
    set(TVBPERF_OPENUSD_INSTALL_DIR ${CMAKE_BINARY_DIR}/external/OpenUSD-install)
    set(TVBPERF_ONE_TBB_BUILD_DIR ${CMAKE_BINARY_DIR}/external/oneTBB-build)
    set(TVBPERF_ONE_TBB_INSTALL_DIR ${CMAKE_BINARY_DIR}/external/oneTBB-install)

    # OpenUSD's direct CMake build requires an installed TBB package. Keep both
    # source trees in external/ so a local checkout is reused when present.
    ExternalProject_Add(OpenUSD_oneTBB
        PREFIX ${CMAKE_BINARY_DIR}/external/OpenUSD_oneTBB-prefix
        SOURCE_DIR ${TVBPERF_ONE_TBB_DIR}
        BINARY_DIR ${TVBPERF_ONE_TBB_BUILD_DIR}
        INSTALL_DIR ${TVBPERF_ONE_TBB_INSTALL_DIR}
        BUILD_BYPRODUCTS
            "${TVBPERF_ONE_TBB_INSTALL_DIR}/lib/tbb12.lib"
            "${TVBPERF_ONE_TBB_INSTALL_DIR}/lib/tbb12_debug.lib"
        GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
        GIT_TAG v2021.9.0
        GIT_SHALLOW TRUE
        UPDATE_DISCONNECTED TRUE
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
            -DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}
            -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
            -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
            # oneTBB v2021.9.0 declares CMake 3.1 compatibility. Newer CMake
            # releases require this explicit compatibility floor.
            -DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5
            -DBUILD_SHARED_LIBS:BOOL=OFF
            -DTBB_TEST:BOOL=OFF
            -DTBB_EXAMPLES:BOOL=OFF
            -DTBB_STRICT:BOOL=OFF
    )

    ExternalProject_Add(OpenUSD
        DEPENDS OpenUSD_oneTBB
        PREFIX ${CMAKE_BINARY_DIR}/external/OpenUSD-prefix
        SOURCE_DIR ${TVBPERF_OPENUSD_DIR}
        BINARY_DIR ${TVBPERF_OPENUSD_BUILD_DIR}
        INSTALL_DIR ${TVBPERF_OPENUSD_INSTALL_DIR}
        BUILD_BYPRODUCTS "${TVBPERF_OPENUSD_INSTALL_DIR}/lib/usd_m.lib"
        GIT_REPOSITORY https://github.com/PixarAnimationStudios/OpenUSD.git
        GIT_TAG v26.05
        GIT_SHALLOW TRUE
        UPDATE_DISCONNECTED TRUE
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
            -DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}
            -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
            -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
            -DTBB_ROOT_DIR:PATH=${TVBPERF_ONE_TBB_INSTALL_DIR}
            -DCMAKE_PREFIX_PATH:PATH=${TVBPERF_ONE_TBB_INSTALL_DIR}
            -DBUILD_SHARED_LIBS:BOOL=OFF
            -DPXR_BUILD_MONOLITHIC:BOOL=ON
            -DPXR_BUILD_TESTS:BOOL=OFF
            -DPXR_BUILD_EXAMPLES:BOOL=OFF
            -DPXR_BUILD_TUTORIALS:BOOL=OFF
            -DPXR_BUILD_USD_TOOLS:BOOL=OFF
            -DPXR_BUILD_IMAGING:BOOL=OFF
            -DPXR_BUILD_USD_IMAGING:BOOL=OFF
            -DPXR_BUILD_USD_VALIDATION:BOOL=OFF
            -DPXR_BUILD_EXEC:BOOL=OFF
            -DPXR_BUILD_USDVIEW:BOOL=OFF
            -DPXR_ENABLE_PYTHON_SUPPORT:BOOL=OFF
            -DPXR_ENABLE_GL_SUPPORT:BOOL=OFF
            -DPXR_ENABLE_PRECOMPILED_HEADERS:BOOL=OFF
    )

    set_target_properties(OpenUSD_oneTBB OpenUSD PROPERTIES FOLDER "External/OpenUSD")
endif()

# ------------------------------------------------------------
# 소스 자동 수집
# ------------------------------------------------------------
file(GLOB_RECURSE TVBPERF_CPP_FILES
    CONFIGURE_DEPENDS
    ${TVBPERF_SRC_DIR}/*.cpp
)

file(GLOB_RECURSE TVBPERF_HEADER_FILES
    CONFIGURE_DEPENDS
    ${TVBPERF_INCLUDE_DIR}/*.h
    ${TVBPERF_INCLUDE_DIR}/*.hpp
)

file(GLOB_RECURSE TVBPERF_RC_FILES
    CONFIGURE_DEPENDS
    ${TVBPERF_SRC_DIR}/*.rc
)

if(TVBPERF_BUILD_OPENUSD)
    set(TVBPERF_SCENE_RAW_JUNGLE_CPP
        ${TVBPERF_SRC_DIR}/scene/raw/SceneRawJungle.cpp
    )
    set(TVBPERF_SCENE_RAW_JUNGLE_TO_SOURCE_CPP
        ${TVBPERF_SRC_DIR}/scene/builder/source/SceneRawJungleToSource.cpp
    )
    list(REMOVE_ITEM TVBPERF_CPP_FILES
        ${TVBPERF_SCENE_RAW_JUNGLE_CPP}
        ${TVBPERF_SCENE_RAW_JUNGLE_TO_SOURCE_CPP}
    )
endif()

# ------------------------------------------------------------
# trashes 디렉터리 제외
# ------------------------------------------------------------
list(FILTER TVBPERF_CPP_FILES EXCLUDE REGEX
    "[/\\\\]trashes[/\\\\]"
)

list(FILTER TVBPERF_HEADER_FILES EXCLUDE REGEX
    "[/\\\\]trashes[/\\\\]"
)

list(FILTER TVBPERF_RC_FILES EXCLUDE REGEX
    "[/\\\\]trashes[/\\\\]"
)

file(GLOB_RECURSE TVBPERF_SHADER_FILES
    CONFIGURE_DEPENDS
    ${TVBPERF_SHADER_DIR}/*.hlsl
    ${TVBPERF_SHADER_DIR}/*.hlsli
    ${TVBPERF_SHADER_DIR}/*.fx
    ${TVBPERF_SHADER_DIR}/*.vs
    ${TVBPERF_SHADER_DIR}/*.ps
    ${TVBPERF_SHADER_DIR}/*.cs
)

# 실행 파일
add_executable(TVBPerf WIN32
    ${TVBPERF_CPP_FILES}
    ${TVBPERF_HEADER_FILES}
    ${TVBPERF_RC_FILES}
    ${TVBPERF_SHADER_FILES}
)

target_compile_features(TVBPerf PRIVATE cxx_std_20)

if(TVBPERF_BUILD_OPENUSD)
    # ExternalProject installs these after generation. CMake validates imported
    # include directories during generation, so ensure their install roots exist.
    file(MAKE_DIRECTORY
        "${TVBPERF_OPENUSD_INSTALL_DIR}/include"
        "${TVBPERF_ONE_TBB_INSTALL_DIR}/include"
    )

    add_library(TVBPerfOpenUSD STATIC IMPORTED GLOBAL)
    set_target_properties(TVBPerfOpenUSD PROPERTIES
        IMPORTED_LOCATION "${TVBPERF_OPENUSD_INSTALL_DIR}/lib/usd_m.lib"
        INTERFACE_INCLUDE_DIRECTORIES
            "${TVBPERF_OPENUSD_INSTALL_DIR}/include;${TVBPERF_ONE_TBB_INSTALL_DIR}/include"
    )
    target_compile_definitions(TVBPerfOpenUSD INTERFACE
        PXR_STATIC
        NOMINMAX
        "$<$<CONFIG:Debug>:TBB_USE_DEBUG>"
    )
    target_link_libraries(TVBPerfOpenUSD INTERFACE
        debug "${TVBPERF_ONE_TBB_INSTALL_DIR}/lib/tbb12_debug.lib"
        optimized "${TVBPERF_ONE_TBB_INSTALL_DIR}/lib/tbb12.lib"
        Shlwapi
        Dbghelp
        Ws2_32
    )
    # usd_m is a static monolith. Keep plugin/schema registration translation
    # units that have no ordinary symbol reference from being discarded.
    target_link_options(TVBPerfOpenUSD INTERFACE
        "$<$<CXX_COMPILER_ID:MSVC>:/WHOLEARCHIVE:${TVBPERF_OPENUSD_INSTALL_DIR}/lib/usd_m.lib>"
    )

    add_library(SceneRawJungle STATIC ${TVBPERF_SCENE_RAW_JUNGLE_CPP})
    target_compile_features(SceneRawJungle PUBLIC cxx_std_20)
    target_include_directories(SceneRawJungle PUBLIC ${TVBPERF_INCLUDE_DIR})
    target_compile_definitions(SceneRawJungle PRIVATE
        "TVBPERF_OPENUSD_PLUGIN_ROOT=\"${TVBPERF_OPENUSD_INSTALL_DIR}/lib/usd\""
        "TVBPERF_OPENUSD_SHADER_PLUGIN_ROOT=\"${TVBPERF_OPENUSD_INSTALL_DIR}/plugin/usd\""
    )
    target_link_libraries(SceneRawJungle PUBLIC TVBPerfOpenUSD)
    add_dependencies(SceneRawJungle OpenUSD)

    # The standalone conversion tools do not link the renderer executable, so
    # keep their semantic validation and bounds implementation self-contained.
    add_library(SceneRawJungleToSource STATIC
        ${TVBPERF_SCENE_RAW_JUNGLE_TO_SOURCE_CPP}
        ${TVBPERF_SRC_DIR}/math/AABB.cpp
        ${TVBPERF_SRC_DIR}/scene/builder/source/SceneSourceSemanticValidator.cpp
        ${TVBPERF_SRC_DIR}/util/Logger.cpp
    )
    target_compile_features(SceneRawJungleToSource PUBLIC cxx_std_20)
    target_include_directories(SceneRawJungleToSource PUBLIC ${TVBPERF_INCLUDE_DIR})
    target_link_libraries(SceneRawJungleToSource PUBLIC SceneRawJungle)
    add_dependencies(SceneRawJungleToSource OpenUSD)

    add_executable(SceneRawJungleInspect
        tools/SceneRawJungleInspect.cpp
    )
    target_compile_features(SceneRawJungleInspect PRIVATE cxx_std_20)
    target_link_libraries(SceneRawJungleInspect PRIVATE SceneRawJungle)
    add_dependencies(SceneRawJungleInspect OpenUSD)

    add_executable(SceneRawJungleToSourceInspect
        tools/SceneRawJungleToSourceInspect.cpp
    )
    target_compile_features(SceneRawJungleToSourceInspect PRIVATE cxx_std_20)
    target_link_libraries(SceneRawJungleToSourceInspect PRIVATE SceneRawJungleToSource)
    add_dependencies(SceneRawJungleToSourceInspect OpenUSD)

    add_executable(SceneRawJungleToSourceTest
        tests/SceneRawJungleToSourceTest.cpp
    )
    target_compile_features(SceneRawJungleToSourceTest PRIVATE cxx_std_20)
    target_compile_definitions(SceneRawJungleToSourceTest PRIVATE
        "SCENE_RAW_JUNGLE_SOURCE_FIXTURE=\"${CMAKE_SOURCE_DIR}/tests/assets/scene_raw_jungle_to_source_fixture.usda\""
    )
    target_link_libraries(SceneRawJungleToSourceTest PRIVATE SceneRawJungleToSource)
    add_dependencies(SceneRawJungleToSourceTest OpenUSD)

    add_executable(JungleSceneCPUBuilderTest
        tests/JungleSceneCPUBuilderTest.cpp
        ${TVBPERF_SRC_DIR}/scene/builder/source/JungleSceneSourceBuilder.cpp
        ${TVBPERF_SRC_DIR}/scene/builder/cpu/JungleSceneCPUBuilder.cpp
        ${TVBPERF_SRC_DIR}/scene/builder/cpu/SceneCPUBuilder.cpp
        ${TVBPERF_SRC_DIR}/scene/builder/cpu/SceneCPUValidator.cpp
        ${TVBPERF_SRC_DIR}/scene/builder/source/SceneSourceDataValidator.cpp
        ${TVBPERF_SRC_DIR}/scene/builder/source/SceneSourceGeometryValidator.cpp
        ${TVBPERF_SRC_DIR}/scene/builder/source/SceneSourceHierarchyValidator.cpp
    )
    target_compile_features(JungleSceneCPUBuilderTest PRIVATE cxx_std_20)
    target_compile_definitions(JungleSceneCPUBuilderTest PRIVATE
        "JUNGLE_SCENE_CPU_SOURCE_FIXTURE=\"${CMAKE_SOURCE_DIR}/tests/assets/scene_raw_jungle_to_source_fixture.usda\""
    )
    target_include_directories(JungleSceneCPUBuilderTest PRIVATE
        ${TVBPERF_INCLUDE_DIR}
    )
    target_link_libraries(JungleSceneCPUBuilderTest PRIVATE
        SceneRawJungleToSource
    )
    add_dependencies(JungleSceneCPUBuilderTest OpenUSD)

    # The runtime Jungle source builder calls the semantic converter, which
    # already links the raw OpenUSD stage target transitively.
    target_link_libraries(TVBPerf PRIVATE SceneRawJungleToSource)
endif()

# include 경로
target_include_directories(TVBPerf
    PRIVATE
        ${TVBPERF_INCLUDE_DIR}
        ${TVBPERF_ASSIMP_DIR}/include
        ${TVBPERF_SHADER_DIR}
)

# 전처리기 정의
target_compile_definitions(TVBPerf
    PRIVATE
        _CONSOLE
        UNICODE
        _UNICODE

        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Release>:NDEBUG>

        $<$<AND:$<PLATFORM_ID:Windows>,$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>>:WIN32>
)

# ------------------------------------------------------------
# MSVC 컴파일 옵션
# ------------------------------------------------------------
if(MSVC)
    target_compile_options(TVBPerf
        PRIVATE
            /W3
            /sdl
            /permissive-

            # Debug 옵션
            $<$<CONFIG:Debug>:/Od>
            $<$<CONFIG:Debug>:/Zi>

            # Release 옵션
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:Release>:/Ob2>
            $<$<CONFIG:Release>:/Oi>
            $<$<CONFIG:Release>:/Ot>
            $<$<CONFIG:Release>:/Gy>
            $<$<CONFIG:Release>:/DNDEBUG>
    )

    # Debug=/MDd, Release=/MD
    set_property(TARGET TVBPerf PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )

    # Release Whole Program Optimization
    set_property(TARGET TVBPerf PROPERTY
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    )
endif()

# 링크 라이브러리
target_link_libraries(TVBPerf
    PRIVATE
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

# ------------------------------------------------------------
# 링커 옵션
# ------------------------------------------------------------
if(MSVC)
    target_link_options(TVBPerf
        PRIVATE
            /SUBSYSTEM:WINDOWS
            /DEBUG

            # Debug
            $<$<CONFIG:Debug>:/INCREMENTAL>

            # Release
            $<$<CONFIG:Release>:/INCREMENTAL:NO>
            $<$<CONFIG:Release>:/OPT:REF>
            $<$<CONFIG:Release>:/OPT:ICF>
            $<$<CONFIG:Release>:/LTCG>
    )
endif()

# ------------------------------------------------------------
# 셰이더는 직접 컴파일하지 않음
# Visual Studio에서도 빌드 도구로 처리하지 않도록 설정
# ------------------------------------------------------------
if(TVBPERF_SHADER_FILES)
    set_source_files_properties(
        ${TVBPERF_SHADER_FILES}
        PROPERTIES
            VS_TOOL_OVERRIDE "None"
            HEADER_FILE_ONLY TRUE
    )
endif()

# ------------------------------------------------------------
# 출력 디렉터리
# Debug/Release 별도 폴더
# ------------------------------------------------------------
set_target_properties(TVBPerf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
)

# ------------------------------------------------------------
# 빌드 결과물에 assets/shaders만 복사
# 전체 assets 복사 금지
# ------------------------------------------------------------
add_custom_target(CopyShaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:TVBPerf>/assets/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${TVBPERF_SHADER_DIR}"
        "$<TARGET_FILE_DIR:TVBPerf>/assets/shaders"
    COMMENT "Copying assets/shaders to output directory"
)

add_dependencies(CopyShaders TVBPerf)

add_custom_command(TARGET TVBPerf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:DXC::dxcompiler>"
        "$<TARGET_FILE_DIR:TVBPerf>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TVBPERF_DXC_ROOT}/bin/${TVBPERF_DXC_ARCH}/dxil.dll"
        "$<TARGET_FILE_DIR:TVBPerf>"
    COMMENT "Copying DXC runtime DLLs to output directory"
)

# 작업 디렉터리 설정
set_property(TARGET TVBPerf PROPERTY
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:TVBPerf>"
)

# ------------------------------------------------------------
# Visual Studio 솔루션 탐색기 필터 정리
# ------------------------------------------------------------
source_group(TREE ${TVBPERF_SRC_DIR}
    PREFIX "Source"
    FILES
        ${TVBPERF_CPP_FILES}
        ${TVBPERF_RC_FILES}
)

source_group(TREE ${TVBPERF_INCLUDE_DIR}
    PREFIX "Include"
    FILES
        ${TVBPERF_HEADER_FILES}
)

if(TVBPERF_SHADER_FILES)
    source_group(TREE ${TVBPERF_SHADER_DIR}
        PREFIX "Shader Files"
        FILES
            ${TVBPERF_SHADER_FILES}
    )
endif()
