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

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#MyModel
add_library(MyModel STATIC 
    src/model.cpp
    src/loot_generator.cpp
    src/extra_data.cpp
    src/extra_data_json.cpp
    src/json_loader.cpp
    src/collision_detector.cpp
)

target_include_directories(MyModel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(MyModel PUBLIC
    CONAN_PKG::boost  # Boost is needed by json_loader/extra_data
)

# game_server
add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/request_handler.cpp
    src/api_handler.cpp
    src/app.cpp
    src/my_logger.cpp
    src/options.cpp
    src/ticker.cpp
    src/serializing_listener.cpp
    src/serialization.cpp
)

target_link_libraries(game_server PRIVATE 
    Threads::Threads
    CONAN_PKG::boost
    MyModel
)

target_compile_options(game_server PRIVATE
    -Wall -Wextra -Wpedantic
    #-ftime-report
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_CXX_FLAGS}")

#Tests
enable_testing()

add_executable(game_server_tests
    tests/test_main.cpp
    tests/extra_data_tests.cpp
    tests/loot_generator_tests.cpp
    tests/collision-detector-tests.cpp
    #tests/state-serialization-tests.cpp
)

target_link_libraries(game_server_tests PRIVATE 
    CONAN_PKG::catch2
    CONAN_PKG::boost
    MyModel
) 

target_compile_options(game_server_tests PRIVATE
    -Wall -Wextra -Wpedantic
    #-ftime-report
)

#include(CTest) creates dir Testing

# Add Conan's module path so we can find 'Catch.cmake' or similar scripts
list(APPEND CMAKE_MODULE_PATH ${CONAN_BUILD_DIRS_CATCH2})

# Try to use Catch2's automatic test discovery
if(EXISTS "${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake")
    include(${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake)
    catch_discover_tests(game_server_tests)
else()
    # Fallback if the CMake script isn't found
    message(STATUS "Catch.cmake not found. Registering simple test.")
    add_test(NAME AllTests COMMAND game_server_tests)
endif()