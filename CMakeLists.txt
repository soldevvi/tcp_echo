cmake_minimum_required(VERSION 3.10)

# Название проекта
project(SimpleTcpApp LANGUAGES CXX)

# Настройка стандарта C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Поиск библиотеки потоков (часто нужна для серверов)
find_package(Threads REQUIRED)

# Добавляем исполняемый файл сервера
add_executable(tcp_server src/server.cpp)
target_link_libraries(tcp_server PRIVATE Threads::Threads)

# Добавляем исполняемый файл клиента
add_executable(tcp_client src/client.cpp)
target_link_libraries(tcp_client PRIVATE Threads::Threads)

# Опционально: вывод подсказки по путям после сборки
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")