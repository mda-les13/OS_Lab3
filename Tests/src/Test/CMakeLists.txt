cmake_minimum_required(VERSION 3.14)
project(MarkerThreadTest)

# Найти установленную библиотеку Boost
find_package(Boost REQUIRED COMPONENTS unit_test_framework)

# Включить Boost в проект
include_directories(${Boost_INCLUDE_DIRS})

# Добавить тестовый исполняемый файл
add_executable(${PROJECT_NAME} tests.cpp)

# Связать с Boost.Test
target_link_libraries(${PROJECT_NAME}
    Boost::unit_test_framework
)

# Добавить тест в CTest
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# Команда для запуска тестов после сборки
add_custom_command(
    TARGET ${PROJECT_NAME}
    COMMENT "Running tests..."
    POST_BUILD
    COMMAND ${PROJECT_NAME}
)