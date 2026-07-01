cmake_minimum_required(VERSION 3.15)

project(HttpServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 递归收集所有源文件
file(GLOB_RECURSE SRC_FILES
    "src/*.cpp"
)

add_executable(httpserver ${SRC_FILES})

target_include_directories(httpserver PRIVATE include)

# 链接 mysql 库(可选)
find_library(MYSQL_LIB mysqlclient)
if(MYSQL_LIB)
    target_link_libraries(httpserver PRIVATE ${MYSQL_LIB})
    target_compile_definitions(httpserver PRIVATE USE_MYSQL)
endif()

target_link_libraries(httpserver PRIVATE pthread)

# 单元测试
add_subdirectory(test)
