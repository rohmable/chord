find_package(GTest)
if(GTEST_FOUND)
    message(STATUS "Using googletest")
else()
    message(STATUS "Getting googletest via FetchContent, this can take a while depending on your internet connection")
    include(FetchContent)
    fetchContent_Declare(
        googletest
        GIT_REPOSITORY  https://github.com/google/googletest
        GIT_TAG         release-1.10.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

set(test_cfg "${CMAKE_SOURCE_DIR}/cfg/cfg.test.json")
set(mock_data "${CMAKE_SOURCE_DIR}/cfg/mock_data.json")
configure_file("${test_cfg}" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)
configure_file("${mock_data}" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)

include_directories("${PROJECT_BINARY_DIR}/src")
set(TEST_BIN chord_test)
set(TEST_SOURCES "main.cpp" "node_test.cpp" "test_client.cpp" "mail_test.cpp" "chord_mail_test.cpp")
add_executable(${TEST_BIN} ${TEST_SOURCES})
add_test(NAME ${TEST_BIN} COMMAND ${TEST_BIN})
target_link_libraries(${TEST_BIN} PUBLIC chord mail GTest::GTest GTest::Main nlohmann_json::nlohmann_json)