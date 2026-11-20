#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include "output/RecordingSidecar.h"

namespace fs = std::filesystem;
using tft::RecordingSidecarData;
using tft::RoundMarker;
using tft::SidecarPathFor;
using tft::WriteRecordingSidecar;

namespace {

RecordingSidecarData ValidData() {
    RecordingSidecarData data;
    data.videoFile = "tft_20261120_194012.mp4";
    data.createdAtUtc = "2026-11-20T22:40:12Z";
    data.durationMs = 93210;
    data.resolution = "1080p";
    data.fps = 60;
    return data;
}

std::string ReadAll(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

void RemoveDir(const fs::path& dir) {
    std::error_code error;
    fs::remove_all(dir, error); // best effort: nunca lança exceção no Windows.
}

fs::path UniqueDir(const std::string& name) {
    auto dir = fs::temp_directory_path() / name;
    RemoveDir(dir);
    fs::create_directories(dir);
    return dir;
}

} // namespace

TEST(RecordingSidecar, WritesExpectedJsonNextToMp4) {
    const auto dir = UniqueDir("tftape_sidecar_basic");
    const auto mp4 = dir / "tft_20261120_194012.mp4";
    const auto data = ValidData();

    std::string error;
    ASSERT_TRUE(WriteRecordingSidecar(data, mp4, error)) << error;
    EXPECT_TRUE(error.empty());

    const auto sidecar = SidecarPathFor(mp4);
    EXPECT_EQ(sidecar.filename().string(), "tft_20261120_194012.mp4.tftape.json");
    ASSERT_TRUE(fs::exists(sidecar));

    const auto json = ReadAll(sidecar);
    EXPECT_NE(json.find("\"schemaVersion\":1"), std::string::npos);
    EXPECT_NE(json.find("\"videoFile\":\"tft_20261120_194012.mp4\""), std::string::npos);
    EXPECT_NE(json.find("\"createdAtUtc\":\"2026-11-20T22:40:12Z\""), std::string::npos);
    EXPECT_NE(json.find("\"durationMs\":93210"), std::string::npos);
    EXPECT_NE(json.find("\"resolution\":\"1080p\""), std::string::npos);
    EXPECT_NE(json.find("\"fps\":60"), std::string::npos);
    EXPECT_NE(json.find("\"rounds\":[]"), std::string::npos);

    RemoveDir(dir);
}

TEST(RecordingSidecar, ReplacesPreviousSidecarAndLeavesNoTempFile) {
    const auto dir = UniqueDir("tftape_sidecar_replace");
    const auto mp4 = dir / "vod.mp4";

    std::string error;
    ASSERT_TRUE(WriteRecordingSidecar(ValidData(), mp4, error)) << error;

    auto data = ValidData();
    RoundMarker round;
    round.label = "2-1";
    round.startMs = 18420;
    round.source = "hud-roi";
    round.confidence = 0.91;
    data.rounds.push_back(round);

    ASSERT_TRUE(WriteRecordingSidecar(data, mp4, error)) << error;

    const auto sidecar = SidecarPathFor(mp4);
    const auto json = ReadAll(sidecar);
    EXPECT_NE(json.find("\"label\":\"2-1\""), std::string::npos);
    EXPECT_NE(json.find("\"startMs\":18420"), std::string::npos);
    EXPECT_NE(json.find("\"source\":\"hud-roi\""), std::string::npos);
    EXPECT_EQ(json.find("\"rounds\":[]"), std::string::npos);

    auto temporary = sidecar;
    temporary += ".tmp";
    EXPECT_FALSE(fs::exists(temporary));

    RemoveDir(dir);
}

TEST(RecordingSidecar, CreatesParentDirectoryWhenNeeded) {
    const auto dir = UniqueDir("tftape_sidecar_nested");
    const auto mp4 = dir / "nested" / "vod.mp4";

    std::string error;
    ASSERT_TRUE(WriteRecordingSidecar(ValidData(), mp4, error)) << error;
    EXPECT_TRUE(fs::exists(SidecarPathFor(mp4)));

    RemoveDir(dir);
}

TEST(RecordingSidecar, RejectsIncompleteDataWithoutWritingFile) {
    const auto dir = UniqueDir("tftape_sidecar_invalid");
    const auto mp4 = dir / "vod.mp4";

    auto data = ValidData();
    data.videoFile.clear();

    std::string error;
    EXPECT_FALSE(WriteRecordingSidecar(data, mp4, error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(fs::exists(SidecarPathFor(mp4)));

    RemoveDir(dir);
}

TEST(RecordingSidecar, RejectsRoundBeyondDuration) {
    const auto dir = UniqueDir("tftape_sidecar_range");
    const auto mp4 = dir / "vod.mp4";

    auto data = ValidData();
    RoundMarker round;
    round.label = "3-2";
    round.startMs = data.durationMs + 1;
    data.rounds.push_back(round);

    std::string error;
    EXPECT_FALSE(WriteRecordingSidecar(data, mp4, error));
    EXPECT_NE(error.find("além da duração"), std::string::npos);

    RemoveDir(dir);
}

TEST(RecordingSidecar, EscapesQuotesBackslashesAndKeepsUtf8) {
    const auto dir = UniqueDir("tftape_sidecar_escape");
    const auto mp4 = dir / "vod.mp4";

    auto data = ValidData();
    data.videoFile = "tft \"a\\b\" \xC3\xA1udio.mp4";

    std::string error;
    ASSERT_TRUE(WriteRecordingSidecar(data, mp4, error)) << error;

    const auto json = ReadAll(SidecarPathFor(mp4));
    EXPECT_NE(json.find("\\\""), std::string::npos);
    EXPECT_NE(json.find("\\\\"), std::string::npos);
    EXPECT_NE(json.find("\xC3\xA1udio"), std::string::npos);

    RemoveDir(dir);
}
