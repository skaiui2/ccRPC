cmake_minimum_required(VERSION 3.10)
project(nodeB C)

file(GLOB NODEB_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

# ---------- 自动收集 common/src/*.c ----------
file(GLOB COMMON_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/*.c
)

add_library(rpc_common_B STATIC
    ${COMMON_SRC}
)

target_include_directories(rpc_common_B PUBLIC
    ../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(rpc_common_B PRIVATE
    RPC_METHODS_XDEF_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/include/rpc_methods_nodeB.xdef\"
)

# ---------- 构建 nodeB ----------
add_executable(nodeB ${NODEB_SRC})

target_include_directories(nodeB PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ../common/include
)

target_compile_definitions(nodeB PRIVATE
    RPC_METHODS_XDEF_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/include/rpc_methods_nodeB.xdef\"
)

target_link_libraries(nodeB rpc_common_B)
