cmake_minimum_required(VERSION 4.0)

add_library(statsgate SHARED)

target_include_directories(statsgate PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(statsgate PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_sources(statsgate PRIVATE
    command.cpp
    dll_client.cpp
    dllmain.cpp
    freestanding_client.cpp
    lua_client.cpp
    mission_hook.cpp
    stat_client.cpp
    statsgate.cpp
    statsgate.pb.cc
    thread_guard.cpp
)

target_include_directories(statsgate PRIVATE ${CMAKE_SOURCE_DIR}/resource)
target_sources(statsgate PRIVATE ${CMAKE_SOURCE_DIR}/resource/statsgate.rc)

target_link_libraries(statsgate PRIVATE compiler-options)

target_link_options(statsgate PRIVATE
    /DELAYLOAD:ExtraUtilities2.dll
    /DELAY:UNLOAD
)

target_link_libraries(statsgate PRIVATE delayimp)
target_link_libraries(statsgate PRIVATE Comctl32) # for task dialogue

add_library(battlezone2 STATIC IMPORTED)
set_target_properties(battlezone2 PROPERTIES 
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/lib/battlezone2.lib"
)

add_library(ExtraUtilities2 SHARED IMPORTED)
set_target_properties(ExtraUtilities2 PROPERTIES 
    IMPORTED_IMPLIB "${CMAKE_SOURCE_DIR}/lib/ExtraUtilities2.lib"
)

target_link_libraries(statsgate PRIVATE
    battlezone2
    ExtraUtilities2
)

find_package(glaze CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)

target_link_libraries(statsgate PRIVATE glaze::glaze)
target_link_libraries(statsgate PRIVATE protobuf::libprotobuf)
