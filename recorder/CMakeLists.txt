cmake_minimum_required(VERSION 3.20)
project(tftape_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TFT_BUILD_TESTS "Build unit tests" ON)

# Mensagens de erro/aviso em português: garante que MSVC interprete e emita UTF-8,
# evitando dupla codificação (ex.: "indisponÃvel") na UI.
if (MSVC)
  add_compile_options(/utf-8)
endif()

add_library(recorder_core
  src/output/OutputPath.cpp
  src/output/RecordingSidecar.cpp
  src/output/RecordingSettings.cpp
  src/capture/WindowRules.cpp
  src/capture/TftWindowFinder.cpp
  src/capture/WgcCaptureSource.cpp
  src/rounds/RoundDetector.cpp
  src/rounds/RoundDetectorConfigLoader.cpp
  src/rounds/OcrRoundDetector.cpp
  src/rounds/OcrRoundDetectorConfigLoader.cpp
  src/rounds/OcrRoundText.cpp
  src/app/CommandProtocol.cpp
  src/app/RecorderController.cpp
  src/app/RecordingSession.cpp
  src/encode/MfMp4Writer.cpp
  src/encode/Mp4Muxer.cpp
  src/encode/VideoEncoderMf.cpp
  src/encode/AudioEncoderMf.cpp
  src/audio/AudioMixer.cpp
  src/audio/AudioRouting.cpp
  src/audio/PcmStreamBuffer.cpp
  src/audio/WasapiAudioClient.cpp
  src/audio/WasapiAppLoopbackSource.cpp
  src/audio/WasapiSystemLoopbackSource.cpp
  src/audio/WasapiMicSource.cpp
  src/util/Logger.cpp
  src/util/Json.cpp
)

target_include_directories(recorder_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(recorder src/main.cpp)
target_link_libraries(recorder PRIVATE recorder_core)

if (WIN32)
  target_link_libraries(recorder_core PUBLIC user32 psapi ole32 oleaut32 mfplat mfreadwrite mfuuid mf evr d3d11 dxgi runtimeobject windowsapp mmdevapi)
  target_compile_definitions(recorder_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if (TFT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
