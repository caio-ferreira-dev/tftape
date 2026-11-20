#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "app/RecorderController.h"
#include "output/RecordingSidecar.h"

namespace fs = std::filesystem;
using tft::RecorderController;
using tft::RecorderState;

TEST(RecorderState, StartStopTransitions) {
    auto dir = fs::temp_directory_path() / "tftape_state";
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
    auto dir = fs::temp_directory_path() / "tftape_state2";
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
    auto dir = fs::temp_directory_path() / "tftape_state3";
    fs::create_directories(dir);
    RecorderController c;
    c.SetStartHookResult(false, "Janela do TFT não encontrada");
    auto r = c.HandleLine("{\"cmd\":\"start\",\"output_dir\":\"" + dir.generic_string() + "\"}");
    ASSERT_FALSE(r.empty());
    EXPECT_NE(r[0].find("Janela do TFT"), std::string::npos);
    EXPECT_EQ(c.GetState(), RecorderState::Error);
    fs::remove_all(dir);
}

TEST(RecorderState, ReportsDetailedErrorForInvalidJson) {
    RecorderController c;
    auto responses = c.HandleLine("{\"cmd\":\"start\",}");
    ASSERT_EQ(responses.size(), 1u);
    EXPECT_NE(responses[0].find("\"ok\":false"), std::string::npos);
    EXPECT_NE(responses[0].find("JSON inválido"), std::string::npos);
    EXPECT_EQ(c.GetState(), RecorderState::Idle);
}

TEST(RecorderState, RejectsInvalidSettingWithDetailedMessage) {
    RecorderController c;
    auto responses = c.HandleLine(
        "{\"cmd\":\"start\",\"output_dir\":\"C:/vods\",\"fps\":\"45\"}");
    ASSERT_EQ(responses.size(), 1u);
    EXPECT_NE(responses[0].find("fps inválido"), std::string::npos);
    EXPECT_EQ(c.GetState(), RecorderState::Idle);
}

TEST(RecorderState, RejectsNonAbsoluteOutputDirectory) {
    RecorderController c;
    auto responses = c.HandleLine("{\"cmd\":\"start\",\"output_dir\":\"vods\"}");
    ASSERT_EQ(responses.size(), 1u);
    EXPECT_NE(responses[0].find("absoluto"), std::string::npos);
    EXPECT_EQ(c.GetState(), RecorderState::Error);
}

TEST(RecorderState, StopWritesSidecarAlongsideMp4) {
    auto dir = fs::temp_directory_path() / "tftape_sidecar_state_stop";
    fs::remove_all(dir);
    fs::create_directories(dir);

    const auto mp4 = dir / "out.mp4";
    RecorderController c;
    c.SetStartHookResult(true, "ok", mp4.string());
    c.HandleLine("{\"cmd\":\"start\",\"output_dir\":\"" + dir.generic_string() + "\"}");

    auto responses = c.HandleLine("{\"cmd\":\"stop\"}");
    ASSERT_FALSE(responses.empty());
    EXPECT_NE(responses[0].find("\"event\":\"recording_stopped\""), std::string::npos);
    EXPECT_NE(responses[0].find("\"sidecar_path\""), std::string::npos);
    EXPECT_NE(responses[0].find("\"duration_ms\":0"), std::string::npos);

    const auto sidecar = tft::SidecarPathFor(mp4);
    ASSERT_TRUE(fs::exists(sidecar));
    EXPECT_EQ(c.LastSidecarPath(), sidecar.string());

    std::ifstream in(sidecar, std::ios::binary);
    std::ostringstream oss;
    oss << in.rdbuf();
    EXPECT_NE(oss.str().find("\"rounds\":[]"), std::string::npos);

    fs::remove_all(dir);
}

TEST(RecorderState, ShutdownAlsoWritesSidecar) {
    auto dir = fs::temp_directory_path() / "tftape_sidecar_state_shutdown";
    fs::remove_all(dir);
    fs::create_directories(dir);

    const auto mp4 = dir / "out.mp4";
    RecorderController c;
    c.SetStartHookResult(true, "ok", mp4.string());
    c.HandleLine("{\"cmd\":\"start\",\"output_dir\":\"" + dir.generic_string() + "\"}");

    auto responses = c.HandleLine("{\"cmd\":\"shutdown\"}");
    ASSERT_FALSE(responses.empty());
    EXPECT_TRUE(c.ShouldExit());
    ASSERT_TRUE(fs::exists(tft::SidecarPathFor(mp4)));

    fs::remove_all(dir);
}

TEST(RecorderState, FailedStartDoesNotWriteSidecar) {
    auto dir = fs::temp_directory_path() / "tftape_sidecar_state_failed";
    fs::remove_all(dir);
    fs::create_directories(dir);

    const auto mp4 = dir / "out.mp4";
    RecorderController c;
    c.SetStartHookResult(false, "Janela do TFT não encontrada");
    c.HandleLine("{\"cmd\":\"start\",\"output_dir\":\"" + dir.generic_string() + "\"}");

    EXPECT_FALSE(fs::exists(tft::SidecarPathFor(mp4)));
    EXPECT_TRUE(c.LastSidecarPath().empty());

    fs::remove_all(dir);
}
