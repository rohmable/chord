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
target_link_libraries(mail ${LIBGCRYPT_LIBRARIES})
target_include_directories(mail PUBLIC "../include/" "../extern/cereal/include/")

add_library(chord STATIC chord.cpp ${ch_proto_srcs} ${ch_grpc_srcs})
target_link_libraries(chord mail ${_GRPC_GRPCPP} ${_PROTOBUF_LIBPROTOBUF} ${LIBGCRYPT_LIBRARIES})
target_include_directories(chord PUBLIC "../include/" "../extern/cereal/include/" "../extern/grpc/include/")

add_executable(chord_server server.cpp)
target_link_libraries(chord_server chord)
target_include_directories(chord_server PUBLIC "../include/")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lmenu")

add_executable(chord_client client.cpp)
target_link_libraries(chord_client chord ${CURSES_LIBRARIES})
target_include_directories(chord_client PUBLIC "../include/" ${CURSES_INCLUDE_DIR})