cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# include 目录
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# 通用源文件（被测试模块共享）
set(COMMON_SRC
    ${CMAKE_SOURCE_DIR}/src/net/Buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/net/InetAddress.cpp
    ${CMAKE_SOURCE_DIR}/src/net/Socket.cpp
    ${CMAKE_SOURCE_DIR}/src/net/Epoller.cpp
    ${CMAKE_SOURCE_DIR}/src/net/Acceptor.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpParser.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpRequest.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpResponse.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpConn.cpp
    ${CMAKE_SOURCE_DIR}/src/pool/threadpool/ThreadPool.cpp
    ${CMAKE_SOURCE_DIR}/src/pool/threadpool/Worker.cpp
    ${CMAKE_SOURCE_DIR}/src/pool/sqlconnpool/SqlConnPool.cpp
    ${CMAKE_SOURCE_DIR}/src/pool/sqlconnpool/Connection.cpp
    ${CMAKE_SOURCE_DIR}/src/timer/HeapTimer.cpp
    ${CMAKE_SOURCE_DIR}/src/log/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/log/AsyncLogger.cpp
    ${CMAKE_SOURCE_DIR}/src/log/LogFile.cpp
    ${CMAKE_SOURCE_DIR}/src/common/Config.cpp
    ${CMAKE_SOURCE_DIR}/src/server/Server.cpp
)

# 查找 MySQL 库
find_library(MYSQL_LIB mysqlclient)

# 链接选项
set(LINK_LIBS pthread)
if(MYSQL_LIB)
    list(APPEND LINK_LIBS ${MYSQL_LIB})
    add_compile_definitions(USE_MYSQL)
endif()

# ==================== Buffer 测试 ====================
add_executable(test_buffer test_buffer.cpp ${CMAKE_SOURCE_DIR}/src/net/Buffer.cpp)
target_include_directories(test_buffer PRIVATE ${INCLUDE_DIR})
target_link_libraries(test_buffer PRIVATE ${LINK_LIBS})

# ==================== Timer 测试 ====================
add_executable(test_timer test_timer.cpp ${CMAKE_SOURCE_DIR}/src/timer/HeapTimer.cpp)
target_include_directories(test_timer PRIVATE ${INCLUDE_DIR})
target_link_libraries(test_timer PRIVATE ${LINK_LIBS})

# ==================== HTTP 测试 ====================
add_executable(test_http test_http.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpParser.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpRequest.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpResponse.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/http/HttpConn.cpp
    ${CMAKE_SOURCE_DIR}/src/net/Buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/pool/sqlconnpool/SqlConnPool.cpp
    ${CMAKE_SOURCE_DIR}/src/pool/sqlconnpool/Connection.cpp
    ${CMAKE_SOURCE_DIR}/src/log/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/log/AsyncLogger.cpp
    ${CMAKE_SOURCE_DIR}/src/log/LogFile.cpp
    ${CMAKE_SOURCE_DIR}/src/net/InetAddress.cpp
    ${CMAKE_SOURCE_DIR}/src/net/Socket.cpp
)
target_include_directories(test_http PRIVATE ${INCLUDE_DIR})
target_link_libraries(test_http PRIVATE ${LINK_LIBS})

# ==================== ThreadPool 测试 ====================
add_executable(test_threadpool test_threadpool.cpp
    ${CMAKE_SOURCE_DIR}/src/pool/threadpool/ThreadPool.cpp
    ${CMAKE_SOURCE_DIR}/src/pool/threadpool/Worker.cpp
    ${CMAKE_SOURCE_DIR}/src/log/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/log/AsyncLogger.cpp
    ${CMAKE_SOURCE_DIR}/src/log/LogFile.cpp
)
target_include_directories(test_threadpool PRIVATE ${INCLUDE_DIR})
target_link_libraries(test_threadpool PRIVATE ${LINK_LIBS})

# ==================== SQL 连接池测试 (需要 MySQL) ====================
if(MYSQL_LIB)
    add_executable(test_sqlconnpool test_sqlconnpool.cpp
        ${CMAKE_SOURCE_DIR}/src/pool/sqlconnpool/SqlConnPool.cpp
        ${CMAKE_SOURCE_DIR}/src/pool/sqlconnpool/Connection.cpp
        ${CMAKE_SOURCE_DIR}/src/log/Logger.cpp
        ${CMAKE_SOURCE_DIR}/src/log/AsyncLogger.cpp
        ${CMAKE_SOURCE_DIR}/src/log/LogFile.cpp
    )
    target_include_directories(test_sqlconnpool PRIVATE ${INCLUDE_DIR})
    target_link_libraries(test_sqlconnpool PRIVATE ${LINK_LIBS})
endif()
