cmake_minimum_required(VERSION 3.10)
project(nodeB C)

file(GLOB NODEB_SRC
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

add_executable(nodeB
    ${NODEB_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/rpc_gen.c
)

target_include_directories(nodeB PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

target_compile_definitions(nodeB PRIVATE
    RPC_METHODS_XDEF_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/include/rpc_methods_nodeB.xdef\"
)

find_package(Threads REQUIRED)

target_link_libraries(nodeB PRIVATE
    rpc_common
    Threads::Threads
)
