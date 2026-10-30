cmake_minimum_required(VERSION 4.0)

add_library(bzreplay STATIC)

target_include_directories(bzreplay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(bzreplay PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_sources(bzreplay PRIVATE
    bzreplay.cpp
)

target_link_libraries(bzreplay PUBLIC compiler-options)

find_path(FASTIO_INCLUDE_DIRS "fast_io.h")
target_include_directories(bzreplay PUBLIC ${FASTIO_INCLUDE_DIRS})

find_package(glm CONFIG REQUIRED)
target_link_libraries(bzreplay PUBLIC glm::glm)

find_path(ZPP_BITS_INCLUDE_DIRS "zpp_bits.h")
target_include_directories(bzreplay PUBLIC ${ZPP_BITS_INCLUDE_DIRS})

find_package(zstd CONFIG REQUIRED)
target_link_libraries(bzreplay PRIVATE zstd::libzstd)

add_executable(replaytest main.cpp)
target_link_libraries(replaytest PUBLIC bzreplay)
target_link_libraries(replaytest PUBLIC compiler-options)
