cmake_minimum_required(VERSION 3.16)

project (   
    Chord
    VERSION 0.1
    LANGUAGES CXX
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(BUILD_TESTS "Build tests made with googletest" ON)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(libgcrypt REQUIRED)
find_package(Threads REQUIRED)
find_package(Qt5 COMPONENTS Widgets REQUIRED)
add_subdirectory("extern/grpc" EXCLUDE_FROM_ALL)
set(_PROTOBUF_LIBPROTOBUF libprotobuf)
set(_REFLECTION grpc++_reflection)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
set(_GRPC_GRPCPP grpc++)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)

find_package(Curses REQUIRED)

add_subdirectory(src)
if(BUILD_TESTS)
    add_subdirectory(test)
endif()

