cmake_minimum_required(VERSION 3.12)

project(CortexServer)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/CortexCommon)

set(CORTEX_SERVER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/sources/server.cpp
)

add_executable(CortexServer
    ${CORTEX_SERVER_SOURCES}
)

add_compile_definitions(CORTEX_SERVER)

target_link_libraries(CortexServer
    CortexCommon
)