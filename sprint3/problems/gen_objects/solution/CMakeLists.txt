cmake_minimum_required(VERSION 3.11)
project(game_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Conan Setup (Standard 'cmake' generator)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

# 2. Find Threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# 3. Executable Definition
add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/model.cpp
    src/json_loader.cpp
    src/request_handler.cpp
    src/api_handler.cpp
    src/app.cpp
    src/my_logger.cpp
    src/options.cpp
    src/ticker.cpp
    src/loot_generator.cpp
    src/extra_data.cpp
    src/extra_data_serialization.cpp
)

target_compile_options(game_server PRIVATE
    -Wall -Wextra -Wpedantic
    #-ftime-report
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_CXX_FLAGS}")

target_link_libraries(game_server PRIVATE 
    Threads::Threads
    CONAN_PKG::boost
)

add_executable(game_server_tests
    src/model.h
    src/model.cpp
    src/loot_generator.h
    src/loot_generator.cpp
    src/tagged.h
    src/extra_data.cpp
    src/extra_data_serialization.cpp
    src/json_loader.cpp
    tests/extra_data_tests.cpp
    tests/loot_generator_tests.cpp
    
)
target_compile_options(game_server_tests PRIVATE
    -Wall -Wextra -Wpedantic
    #-ftime-report
)
#target_include_directories(game_server_tests PRIVATE CONAN_PKG::boost)
target_link_libraries(game_server_tests PRIVATE 
    CONAN_PKG::catch2
    CONAN_PKG::boost
    )