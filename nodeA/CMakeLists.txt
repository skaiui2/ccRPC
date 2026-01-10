cmake_minimum_required(VERSION 3.10)
project(nodeA C)

file(GLOB NODEA_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

file(GLOB COMMON_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/*.c
)

add_library(rpc_common_A STATIC
    ${COMMON_SRC}
)

target_include_directories(rpc_common_A PUBLIC
    ../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(rpc_common_A PRIVATE
    RPC_METHODS_XDEF_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/include/rpc_methods_nodeA.xdef\"
)

add_executable(nodeA ${NODEA_SRC})

target_include_directories(nodeA PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ../common/include
)

target_compile_definitions(nodeA PRIVATE
    RPC_METHODS_XDEF_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/include/rpc_methods_nodeA.xdef\"
)

target_link_libraries(nodeA rpc_common_A)
