cmake_minimum_required(VERSION 3.10)
project(nodeB C)

# nodeB 源文件
file(GLOB NODEB_SRC
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

# nodeB 编译 rpc_gen.c（生成器）
add_executable(nodeB
    ${NODEB_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/rpc_gen.c
)

target_include_directories(nodeB PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
)

# 关键：告诉生成器使用 nodeB 的 xdef
target_compile_definitions(nodeB PRIVATE
    RPC_METHODS_XDEF_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/include/rpc_methods_nodeB.xdef\"
)

target_link_libraries(nodeB rpc_common)
