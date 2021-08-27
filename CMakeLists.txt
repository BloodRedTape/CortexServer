cmake_minimum_required(VERSION 3.10)

project(CortexServer)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/CortexCommon)

set(CORTEX_SERVER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/sources/server.cpp
)

add_executable(CortexServer
    ${CORTEX_SERVER_SOURCES}
)

target_link_libraries(CortexServer
    CortexCommon
)