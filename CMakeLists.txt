cmake_minimum_required(VERSION 3.21)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/xiao/version.hh" xiao_version_lines
        REGEX "^#define[ \t]+XIAO_VERSION_(MAJOR|MINOR|PATCH)[ \t]+[0-9]+")

foreach (line IN LISTS xiao_version_lines)
    string(REGEX MATCH "XIAO_VERSION_([A-Z]+)[ \t]+([0-9]+)" _ "${line}")
    set(xiao_version_${CMAKE_MATCH_1} ${CMAKE_MATCH_2})
endforeach ()

if (NOT DEFINED xiao_version_MAJOR OR NOT DEFINED xiao_version_MINOR OR NOT DEFINED xiao_version_PATCH)
    message(FATAL_ERROR "xiao: could not read XIAO_VERSION_MAJOR, _MINOR and _PATCH from include/xiao/version.hh")
endif ()

project(xiao
        VERSION ${xiao_version_MAJOR}.${xiao_version_MINOR}.${xiao_version_PATCH}
        LANGUAGES C CXX)

message(STATUS "xiao version: ${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(XIAO_EXAMPLE "build the xiao example executable" ${PROJECT_IS_TOP_LEVEL})
option(XIAO_DISASM "fetch zydis and build the xiao-disasm code dumper into the example" ${PROJECT_IS_TOP_LEVEL})
option(XIAO_TESTS "fetch catch2 and build the xiao test suite" ${PROJECT_IS_TOP_LEVEL})
option(XIAO_ISAGEN "build xiao-isagen and regenerate the x86 instruction tables" ${PROJECT_IS_TOP_LEVEL})
option(XIAO_INSTALL "generate install rules and the xiao CMake package" ${PROJECT_IS_TOP_LEVEL})

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

if (MSVC AND PROJECT_IS_TOP_LEVEL)
    add_compile_options(/MP)
endif ()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(GNUInstallDirs)

if (XIAO_ISAGEN)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ISAGen.cmake")
    add_subdirectory(tools/xiao-isagen)
endif ()

set(XIAO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

add_library(xiao_headers INTERFACE)
add_library(xiao::xiao ALIAS xiao_headers)
set_target_properties(xiao_headers PROPERTIES EXPORT_NAME xiao)

target_include_directories(xiao_headers INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(xiao_headers INTERFACE cxx_std_23)
target_link_libraries(xiao_headers INTERFACE ${CMAKE_DL_LIBS})

file(GLOB XIAO_X86_ISA CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/xiao/isa/x86/inst/defs/*.isa")

if (TARGET xiao-isagen AND XIAO_X86_ISA)
    set(XIAO_X86_GEN_TESTS "${XIAO_GEN_DIR}/tests/x86_insts_test.cc")

    xiao_isagen(xiao_x86_gen
            HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/xiao/isa/x86/inst/gen/insts.hh"
            TESTS "${XIAO_X86_GEN_TESTS}"
            INPUTS ${XIAO_X86_ISA}
    )

    add_dependencies(xiao_headers xiao_x86_gen)
elseif (TARGET xiao-isagen)
    message(STATUS "xiao-isagen: no .isa files in include/xiao/isa/x86/inst/defs yet, x86 generation is off")
endif ()

if (XIAO_DISASM)
    include(FetchContent)

    FetchContent_Declare(zydis
            URL https://github.com/zyantific/zydis/releases/download/v4.1.0/zydis-amalgamated.tar.gz
            URL_HASH SHA256=aa9b82be3a37a2998bd8e16cf583bbf2b6c3d80e97dc20504169dc32ca1ced59
    )
    FetchContent_MakeAvailable(zydis)

    add_library(zydis STATIC "${zydis_SOURCE_DIR}/Zydis.c")
    target_include_directories(zydis PUBLIC "${zydis_SOURCE_DIR}")
    target_compile_definitions(zydis PUBLIC ZYDIS_STATIC_BUILD ZYCORE_STATIC_BUILD)
    set_target_properties(zydis PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
endif ()

if (XIAO_EXAMPLE)
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

    target_link_libraries(${PROJECT_NAME} PRIVATE xiao::xiao)

    if (TARGET zydis)
        target_link_libraries(${PROJECT_NAME} PRIVATE zydis)
        target_compile_definitions(${PROJECT_NAME} PRIVATE XIAO_HAS_ZYDIS)
    endif ()

    set_target_properties(${PROJECT_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/exe"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib"
    )
endif ()

if (XIAO_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

if (XIAO_INSTALL)
    include(CMakePackageConfigHelpers)

    set(XIAO_CMAKE_DIR "${CMAKE_INSTALL_DATADIR}/xiao/cmake")

    install(TARGETS xiao_headers EXPORT xiaoTargets)

    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            FILES_MATCHING
            PATTERN "*.hh"
            PATTERN "xiao-disasm" EXCLUDE
            PATTERN "defs" EXCLUDE
    )

    install(EXPORT xiaoTargets
            NAMESPACE xiao::
            DESTINATION "${XIAO_CMAKE_DIR}"
    )

    configure_package_config_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/xiaoConfig.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/xiaoConfig.cmake"
            INSTALL_DESTINATION "${XIAO_CMAKE_DIR}"
    )

    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/xiaoConfigVersion.cmake"
            COMPATIBILITY SameMinorVersion
            ARCH_INDEPENDENT
    )

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/xiaoConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/xiaoConfigVersion.cmake"
            DESTINATION "${XIAO_CMAKE_DIR}"
    )
endif ()
