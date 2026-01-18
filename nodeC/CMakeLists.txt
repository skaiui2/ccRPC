cmake_minimum_required(VERSION 3.10)
project(nodeC C)

set(CMAKE_BUILD_TYPE Debug)

file(GLOB NODEC_SRC
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

add_executable(nodeC
    ${NODEC_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/rpc_gen.c
)

target_include_directories(nodeC PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

target_compile_definitions(nodeC PRIVATE
    RPC_METHODS_XDEF_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/include/rpc_methods_nodeC.xdef\"
)

find_package(Threads REQUIRED)

target_link_libraries(nodeC PRIVATE
    rpc_common
    Threads::Threads
)
