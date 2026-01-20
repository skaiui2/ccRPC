cmake_minimum_required(VERSION 3.10)
project(nodeA C)

set(CMAKE_BUILD_TYPE Debug)

file(GLOB NODEA_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

file(GLOB COMMON_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/*.c
)
list(FILTER COMMON_SRC EXCLUDE REGEX "rpc_gen\\.c$")

add_library(rpc_common STATIC
    ${COMMON_SRC}
)

target_include_directories(rpc_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

add_executable(nodeA
    ${NODEA_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/rpc_gen.c
)

target_include_directories(nodeA PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

target_compile_definitions(nodeA PRIVATE
    RPC_METHODS_XDEF_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/include/rpc_methods_nodeA.xdef\"
)

find_package(Threads REQUIRED)

target_link_libraries(nodeA PRIVATE
    rpc_common
    Threads::Threads
    rt
)

target_link_libraries(rpc_common PUBLIC rt)
