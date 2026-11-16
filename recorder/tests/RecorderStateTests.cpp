#include <gtest/gtest.h>

#include <filesystem>

#include "app/RecorderController.h"

namespace fs = std::filesystem;
using tft::RecorderController;
using tft::RecorderState;

TEST(RecorderState, StartStopTransitions) {
    auto dir = fs::temp_directory_path() / "tft_vod_recorder_state";
    fs::create_directories(dir);
    RecorderController c;
    c.SetStartHookResult(true, "ok", (dir / "out.mp4").string());
    auto r1 = c.HandleLine("{\"cmd\":\"start\",\"output_dir\":\"" + dir.generic_string() + "\",\"resolution\":\"720p\",\"fps\":\"30\"}");
    ASSERT_FALSE(r1.empty());
    EXPECT_EQ(c.GetState(), RecorderState::Recording);
    auto r2 = c.HandleLine("{\"cmd\":\"stop\"}");
    ASSERT_FALSE(r2.empty());
    EXPECT_EQ(c.GetState(), RecorderState::Idle);
    fs::remove_all(dir);
}

TEST(RecorderState, RejectsStartWhenAlreadyRecording) {
    auto dir = fs::temp_directory_path() / "tft_vod_recorder_state2";
    fs::create_directories(dir);
    RecorderController c;
    c.SetStartHookResult(true, "ok", (dir / "out.mp4").string());
    c.HandleLine("{\"cmd\":\"start\",\"output_dir\":\"" + dir.generic_string() + "\"}");
    auto r = c.HandleLine("{\"cmd\":\"start\",\"output_dir\":\"" + dir.generic_string() + "\"}");
    ASSERT_FALSE(r.empty());
    EXPECT_NE(r[0].find("\"ok\":false"), std::string::npos);
    EXPECT_EQ(c.GetState(), RecorderState::Recording);
    fs::remove_all(dir);
}

TEST(RecorderState, RejectsStopWhenIdle) {
    RecorderController c;
    auto r = c.HandleLine("{\"cmd\":\"stop\"}");
    ASSERT_FALSE(r.empty());
    EXPECT_NE(r[0].find("\"ok\":false"), std::string::npos);
    EXPECT_EQ(c.GetState(), RecorderState::Idle);
}

TEST(RecorderState, ReportsErrorOnFailedStart) {
    auto dir = fs::temp_directory_path() / "tft_vod_recorder_state3";
    fs::create_directories(dir);
    RecorderController c;
    c.SetStartHookResult(false, "Janela do TFT não encontrada");
    auto r = c.HandleLine("{\"cmd\":\"start\",\"output_dir\":\"" + dir.generic_string() + "\"}");
    ASSERT_FALSE(r.empty());
    EXPECT_NE(r[0].find("Janela do TFT"), std::string::npos);
    EXPECT_EQ(c.GetState(), RecorderState::Error);
    fs::remove_all(dir);
}
