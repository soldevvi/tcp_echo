cmake_minimum_required(VERSION 3.10)

# Название проекта
project(SimpleTcpApp LANGUAGES CXX)

# Настройка стандарта C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Поиск библиотеки потоков (часто нужна для серверов)
find_package(Threads REQUIRED)

# add_dependencies(tcp_server src/metric.hpp)

# Добавляем исполняемый файл сервера
add_executable(tcp_server src/server.cpp)
add_executable(tcp_server_old src/server_prev.cpp)
add_executable(tcp_server_b src/server.cpp)
target_link_libraries(tcp_server PRIVATE Threads::Threads)
target_link_libraries(tcp_server_old PRIVATE Threads::Threads)
target_link_libraries(tcp_server_b PRIVATE Threads::Threads)
# Добавляем исполняемый файл клиента
add_executable(tcp_client src/client.cpp)
target_link_libraries(tcp_client PRIVATE Threads::Threads)

# Генераторное выражение: Добавить макрос ENABLE_DEBUG_LOG только если CONFIG это Debug
target_compile_definitions(tcp_server PRIVATE 
    $<$<CONFIG:Debug>:ENABLE_DEBUG_LOG>
)
# -------------------------------

# Опционально: вывод подсказки по путям после сборки
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")