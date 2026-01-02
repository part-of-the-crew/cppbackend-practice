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
    src/http_server.h
    src/sdk.h
    src/model.h
    src/model.cpp
    src/tagged.h
    src/json_loader.h
    src/json_loader.cpp
    src/request_handler.cpp
    src/request_handler.h
)


#target_link_libraries(game_server PRIVATE 
#    Threads::Threads
##    CONAN_PKG::boost
#    ${CONAN_LIBS_BOOST_JSON}
#    ${CONAN_LIBS_BOOST_LOG_SETUP}
 #   ${CONAN_LIBS_BOOST_LOG}
#    ${CONAN_LIBS_BOOST_FILESYSTEM}
#)
target_link_libraries(game_server PRIVATE 
    Threads::Threads
    CONAN_PKG::boost
    # When linking static Boost Log, you often need these system libs
    dl
    rt
)