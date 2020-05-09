cmake_minimum_required(VERSION 3.16)

project (   
    Chord
    VERSION 0.1
    LANGUAGES CXX
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
option(BUILD_TESTS "Build tests made with googletest" ON)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(FetchContent)
find_package(Threads REQUIRED)
find_package(libgcrypt REQUIRED)
find_package(Protobuf CONFIG)
find_package(gRPC)
if(${gRPC_FOUND})
    set(protobuf_MODULE_COMPATBILE TRUE)
    message(STATUS "Using Protobuf ${Protobuf_VERSION}")
    message(STATUS "Using gRPC ${gRPC_VERSION}")
    set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
    set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)

    set(_GRPC_GRPCPP_UNSECURE gRPC::grpc++_unsecure)
    set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
else()
    message(STATUS "Getting gRPC via FetchContent, this can take a while depending on your internet connection")
    FetchContent_Declare(
        gRPC
        GIT_REPOSITORY  https://github.com/grpc/grpc
        GIT_TAG         v1.27.3
    )
    FetchContent_MakeAvailable(gRPC)

    set(_PROTOBUF_LIBPROTOBUF libprotobuf)
    set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
    set(_GRPC_GRPCPP_UNSECURE grpc++_unsecure)
    set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
endif()

find_package(nlohmann_json 3.2.0)
if(${nlohmann_json_FOUND})
    message(STATUS "Using nlohmann_json ${nlohmann_json_VERSION}")
else()
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    message(STATUS "Getting nlohmann_json via FetchContent, this can take a while depending on your internet connection")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY  https://github.com/nlohmann/json
        GIT_TAG         v3.7.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_subdirectory(src)
if(BUILD_TESTS)
    add_subdirectory(test)
endif()

