include(FetchContent)

FetchContent_Declare(catch2
    URL      https://github.com/catchorg/Catch2/archive/refs/tags/v3.7.1.tar.gz
    URL_HASH SHA256=c991b247a1a0d7bb9c39aa35faf0fe9e19764213f28ffba3109388e62ee0269c
)
FetchContent_MakeAvailable(catch2)

file(GLOB XIAO_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hh"
)

add_executable(xiao_tests ${XIAO_TEST_SOURCES})

target_include_directories(xiao_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(xiao_tests PRIVATE xiao::xiao Catch2::Catch2WithMain)

if (XIAO_X86_GEN_TESTS)
    target_sources(xiao_tests PRIVATE "${XIAO_X86_GEN_TESTS}")
endif()

if (TARGET zydis)
    target_link_libraries(xiao_tests PRIVATE zydis)
    target_compile_definitions(xiao_tests PRIVATE XIAO_HAS_ZYDIS)
endif()

include("${catch2_SOURCE_DIR}/extras/Catch.cmake")
catch_discover_tests(xiao_tests DISCOVERY_MODE PRE_TEST)
