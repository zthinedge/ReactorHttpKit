cmake_minimum_required(VERSION 3.16)

project(ReactorHttpKit
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(REACTOR_HTTP_KIT_BUILD_EXAMPLES "Build ReactorHttpKit examples" ON)

file(GLOB_RECURSE REACTOR_HTTP_KIT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/net/*.cpp"
)

add_library(reactor_http_kit STATIC ${REACTOR_HTTP_KIT_SOURCES})

target_include_directories(reactor_http_kit
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_options(reactor_http_kit
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_link_libraries(reactor_http_kit
    PUBLIC
        pthread
)

if(REACTOR_HTTP_KIT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
