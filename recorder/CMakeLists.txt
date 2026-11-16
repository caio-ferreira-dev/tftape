cmake_minimum_required(VERSION 3.20)
project(tft_vod_recorder_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TFT_BUILD_TESTS "Build unit tests" ON)

add_library(recorder_core
  src/output/OutputPath.cpp
  src/output/RecordingSettings.cpp
  src/capture/WindowRules.cpp
  src/capture/TftWindowFinder.cpp
  src/app/CommandProtocol.cpp
  src/app/RecorderController.cpp
)

target_include_directories(recorder_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(recorder src/main.cpp)
target_link_libraries(recorder PRIVATE recorder_core)

if (WIN32)
  target_link_libraries(recorder_core PUBLIC user32 psapi mfplat mfreadwrite mfuuid mf windowscodecs d3d11 dxgi runtimeobject)
  target_compile_definitions(recorder_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if (TFT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
