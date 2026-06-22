cmake_minimum_required(VERSION 3.20)

project(news_server VERSION 0.1.0 LANGUAGES CXX)

option(NEWS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(news_core
    src/config.cpp
    src/epoll_server.cpp
    src/protocol.cpp
    src/replay_ring.cpp
    src/session.cpp
    src/wal.cpp
)

target_include_directories(news_core PUBLIC include)

target_compile_options(news_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
)

if(NEWS_ENABLE_SANITIZERS)
    target_compile_options(news_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(news_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(news_server src/server_main.cpp)
target_link_libraries(news_server PRIVATE news_core)

add_executable(news_client client/client_main.cpp)
target_link_libraries(news_client PRIVATE news_core)

include(CTest)

if(BUILD_TESTING)
    foreach(test_name IN ITEMS protocol wal replay_ring integration)
        add_executable(${test_name}_tests tests/${test_name}_tests.cpp)
        target_link_libraries(${test_name}_tests PRIVATE news_core)
        add_test(NAME ${test_name}_tests COMMAND ${test_name}_tests)
    endforeach()
endif()
