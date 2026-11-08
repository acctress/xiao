cmake_minimum_required(VERSION 3.21)

get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

project(${PROJECT_NAME} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(XIAO_DISASM "fetch zydis and build the xiao-disasm code dumper into the example" ON)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

if (MSVC)
    add_compile_options(/MP)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file(GLOB_RECURSE PROJ_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

file(GLOB_RECURSE PROJ_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cxx"
)

add_executable(${PROJECT_NAME} ${PROJ_SOURCES} ${PROJ_HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if (XIAO_DISASM)
    include(FetchContent)

    FetchContent_Declare(zydis
        URL      https://github.com/zyantific/zydis/releases/download/v4.1.0/zydis-amalgamated.tar.gz
        URL_HASH SHA256=aa9b82be3a37a2998bd8e16cf583bbf2b6c3d80e97dc20504169dc32ca1ced59
    )
    FetchContent_MakeAvailable(zydis)

    add_library(zydis STATIC "${zydis_SOURCE_DIR}/Zydis.c")
    target_include_directories(zydis PUBLIC "${zydis_SOURCE_DIR}")
    target_compile_definitions(zydis PUBLIC ZYDIS_STATIC_BUILD ZYCORE_STATIC_BUILD)
    set_target_properties(zydis PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

    target_link_libraries(${PROJECT_NAME} PRIVATE zydis)
    target_compile_definitions(${PROJECT_NAME} PRIVATE XIAO_HAS_ZYDIS)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/exe"    
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib"    
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib"    
)
