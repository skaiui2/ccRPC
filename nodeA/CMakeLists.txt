cmake_minimum_required(VERSION 3.10)
project(nodeA C)

set(CMAKE_BUILD_TYPE Debug)

file(GLOB NODEA_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

file(GLOB PORT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../port/src/*.c)
file(GLOB CCRPC_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../ccrpc/source/*.c)
file(GLOB LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../lib/source/*.c)
file(GLOB MG_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../mg/source/*.c)

add_library(rpc_common STATIC
    ${PORT_SRC}
    ${CCRPC_SRC}
    ${LIB_SRC}
    ${MG_SRC}
)

target_include_directories(rpc_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../port/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ccrpc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mg/include
)

add_executable(nodeA
    ${NODEA_SRC}
)

target_include_directories(nodeA PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../port/include
)

target_compile_definitions(rpc_common PUBLIC
    RPC_METHODS_XDEF_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/include/rpc_methods_nodeA.xdef\"
)

find_package(Threads REQUIRED)

target_link_libraries(nodeA PRIVATE
    rpc_common
    Threads::Threads
    rt
)

target_link_libraries(rpc_common PUBLIC rt)
