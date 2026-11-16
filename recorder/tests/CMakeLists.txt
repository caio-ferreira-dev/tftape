include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(recorder_tests
  WindowRulesMatchTests.cpp
  WindowRulesConfigTests.cpp
  OutputPathTests.cpp
  RecordingSettingsTests.cpp
  CommandProtocolTests.cpp
  RecorderStateTests.cpp
)

target_link_libraries(recorder_tests PRIVATE recorder_core GTest::gtest_main)
target_include_directories(recorder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

include(GoogleTest)
gtest_discover_tests(recorder_tests)
