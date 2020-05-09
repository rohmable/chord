get_filename_component(ch_proto "../protos/chord.proto" ABSOLUTE)
get_filename_component(ch_proto_path "${ch_proto}" PATH)

set(ch_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/chord.pb.cc")
set(ch_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/chord.pb.h")
set(ch_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/chord.grpc.pb.cc")
set(ch_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/chord.grpc.pb.h")
add_custom_command(
    OUTPUT "${ch_proto_srcs}" "${ch_proto_hdrs}" "${ch_grpc_srcs}" "${ch_grpc_hdrs}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
         --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
         -I "${ch_proto_path}"
         --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
         "${ch_proto}"
    DEPENDS "${ch_proto}")

set(ch_cfg "${CMAKE_SOURCE_DIR}/cfg/cfg.json")
configure_file("${ch_cfg}" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_library(mail STATIC mail.cpp)
target_link_libraries(mail nlohmann_json::nlohmann_json)
target_include_directories(mail PUBLIC "../include/")

add_library(chord STATIC chord.cpp ${ch_proto_srcs} ${ch_grpc_srcs})
target_link_libraries(chord mail ${_GRPC_GRPCPP_UNSECURE} ${_PROTOBUF_LIBPROTOBUF} nlohmann_json::nlohmann_json ${LIBGCRYPT_LIBRARIES})
target_include_directories(chord PUBLIC "../include/")

add_executable(chord_server main.cpp)
target_link_libraries(chord_server chord)
target_include_directories(chord_server PUBLIC "../include/")
