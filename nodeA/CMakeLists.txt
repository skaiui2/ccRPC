cmake_minimum_required(VERSION 3.10)
project(nodeA C)

# nodeA 源文件
file(GLOB NODEA_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

# common runtime（不包含 rpc_gen.c）
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

# nodeA 编译 rpc_gen.c（生成器输出）
add_executable(nodeA
    ${NODEA_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/rpc_gen.c
)

target_include_directories(nodeA PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# 关键：告诉生成器使用 nodeA 的 xdef
target_compile_definitions(nodeA PRIVATE
    RPC_METHODS_XDEF_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/include/rpc_methods_nodeA.xdef\"
)

target_link_libraries(nodeA rpc_common)
