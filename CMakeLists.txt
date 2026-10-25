cmake_minimum_required(VERSION 4.0)

project(statsgate LANGUAGES CXX)

add_library(compiler-options INTERFACE)

target_include_directories(compiler-options INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
)

target_compile_features(compiler-options INTERFACE cxx_std_23)

if(MSVC)
    target_compile_options(compiler-options INTERFACE
        /W4 /WX /permissive- /external:anglebrackets /external:W0
    )
else()
    message(FATAL_ERROR "MSVC is required to build this project")
endif()

add_subdirectory(statslauncher)
add_subdirectory(statsrecorder)
