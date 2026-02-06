cmake_minimum_required(VERSION 3.11)
project(book_manager CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Conan Setup (Standard 'cmake' generator)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS NO_OUTPUT_DIRS)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# 2. Find Threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#MyModel
add_library(MyModel STATIC 
    src/json_loader.cpp
)

target_include_directories(MyModel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(MyModel PUBLIC
    CONAN_PKG::boost  # Boost is needed by json_loader
)

# game_server
add_executable(book_manager
    src/main.cpp
)

target_link_libraries(book_manager PRIVATE 
    Threads::Threads
    CONAN_PKG::boost
    CONAN_PKG::libpqxx
    MyModel
)

target_compile_options(book_manager PRIVATE
    -Wall -Wextra -Wpedantic
    #-ftime-report
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_CXX_FLAGS}")

#Tests
enable_testing()

add_executable(book_manager_tests
    tests/test_main.cpp
    tests/tests.cpp
)

target_link_libraries(book_manager_tests PRIVATE 
    CONAN_PKG::catch2
    CONAN_PKG::boost
    MyModel
) 

target_compile_options(book_manager_tests PRIVATE
    -Wall -Wextra -Wpedantic
    #-ftime-report
)

#include(CTest) creates dir Testing

# Add Conan's module path so we can find 'Catch.cmake' or similar scripts
list(APPEND CMAKE_MODULE_PATH ${CONAN_BUILD_DIRS_CATCH2})

# Try to use Catch2's automatic test discovery
if(EXISTS "${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake")
    include(${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake)
    catch_discover_tests(book_manager_tests)
else()
    # Fallback if the CMake script isn't found
    message(STATUS "Catch.cmake not found. Registering simple test.")
    add_test(NAME AllTests COMMAND book_manager_tests)
endif()