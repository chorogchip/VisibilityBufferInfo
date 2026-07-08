cmake_minimum_required(VERSION 3.20)

project(TVBPerf LANGUAGES CXX RC)

# Visual Studio 멀티 구성 generator용 구성 명시
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
endif()

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Visual Studio 폴더 정리용
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 경로
set(TVBPERF_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/TVBPerf)
set(TVBPERF_ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets)
set(TVBPERF_SHADER_DIR ${TVBPERF_ASSETS_DIR}/shaders)
set(TVBPERF_EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)
set(TVBPERF_ASSIMP_DIR ${TVBPERF_EXTERNAL_DIR}/assimp)

# ------------------------------------------------------------
# Assimp
# ------------------------------------------------------------
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)

# Exporter 필요 없으면 OFF
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)

# static library
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(${TVBPERF_ASSIMP_DIR})

set_target_properties(assimp PROPERTIES FOLDER "External/assimp")

if(TARGET zlibstatic)
    set_target_properties(zlibstatic PROPERTIES FOLDER "External/assimp")
endif()

if(TARGET UpdateAssimpLibsDebugSymbolsAndDLLs)
    set_target_properties(UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTIES FOLDER "External/assimp")
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
    ${TVBPERF_SRC_DIR}/*.h
    ${TVBPERF_SRC_DIR}/*.hpp
)

file(GLOB_RECURSE TVBPERF_RC_FILES
    CONFIGURE_DEPENDS
    ${TVBPERF_SRC_DIR}/*.rc
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

target_compile_features(TVBPerf PRIVATE cxx_std_17)

# include 경로
target_include_directories(TVBPerf
    PRIVATE
        ${TVBPERF_SRC_DIR}
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
        d3d12
        dxgi
        d3dcompiler
        user32
        gdi32
        kernel32
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
        ${TVBPERF_HEADER_FILES}
        ${TVBPERF_RC_FILES}
)

if(TVBPERF_SHADER_FILES)
    source_group(TREE ${TVBPERF_SHADER_DIR}
        PREFIX "Shader Files"
        FILES
            ${TVBPERF_SHADER_FILES}
    )
endif()