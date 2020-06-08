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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(MAIL_INCLUDE_DIR "../include/mail")
add_library(mail STATIC mail.cpp)
target_link_libraries(mail ${LIBGCRYPT_LIBRARIES})
target_include_directories(mail PUBLIC ${MAIL_INCLUDE_DIR} "../extern/cereal/include/")

set(CHORD_INCLUDE_DIR "../include/chord")
add_library(chord STATIC server.cpp client.cpp ${ch_proto_srcs} ${ch_grpc_srcs})
target_link_libraries(chord mail ${_GRPC_GRPCPP} ${_PROTOBUF_LIBPROTOBUF} ${LIBGCRYPT_LIBRARIES} ${CURSES_LIBRARIES})
target_include_directories(chord PUBLIC "../include/" ${CHORD_INCLUDE_DIR} "../extern/cereal/include/" "../extern/grpc/include/" ${CURSES_INCLUDE_DIR})

add_executable(chord_server chord_server.cpp)
target_link_libraries(chord_server chord)
target_include_directories(chord_server PUBLIC "../include/")

add_executable(node_server node_server.cpp)
target_link_libraries(node_server chord)
target_include_directories(node_server PUBLIC "../include")

add_executable(load_mock_data load_mock_data.cpp)
target_link_libraries(load_mock_data chord)
target_include_directories(load_mock_data PUBLIC "../include" "../extern/cereal/include/")

set(GUI_SRCS clientmainwindow.cpp clientlogin.cpp mailwidget.cpp clientsend.cpp)
set(GUI_HEADERS ../include/gui/clientmainwindow.hpp ../include/gui/clientlogin.hpp ../include/gui/mailwidget.hpp ../include/gui/clientsend.hpp)
set(GUI_UI clientmainwindow.ui mailwidget.ui clientlogin.ui clientsend.ui)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

qt5_wrap_cpp(chord_client_SRC ${GUI_HEADERS})
qt5_wrap_ui(chord_client_UI ${GUI_UI})

add_executable(chord_client ${GUI_SRCS} ${chord_client_SRC} ${chord_client_UI})
target_link_libraries(chord_client chord Qt5::Widgets)
target_include_directories(chord_client PUBLIC "../include/")