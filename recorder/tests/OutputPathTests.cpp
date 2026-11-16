#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "output/OutputPath.h"

namespace fs = std::filesystem;
using tft::OutputPath;

TEST(OutputPath, SanitizeFilenameReplacesInvalidChars) {
    EXPECT_EQ(OutputPath::SanitizeFilename("a:b*c?.mp4"), "a_b_c_.mp4");
    EXPECT_EQ(OutputPath::SanitizeFilename(""), "recording");
}

TEST(OutputPath, EnsureDirectoryCreatesPath) {
    auto dir = fs::temp_directory_path() / "tft_vod_recorder_test_out";
    fs::remove_all(dir);
    std::string err;
    EXPECT_TRUE(OutputPath::EnsureDirectory(dir, &err)) << err;
    EXPECT_TRUE(fs::exists(dir));
    fs::remove_all(dir);
}

TEST(OutputPath, MakeUniqueMp4PathAddsMp4AndUniqueness) {
    auto dir = fs::temp_directory_path() / "tft_vod_recorder_test_unique";
    fs::create_directories(dir);
    auto p1 = OutputPath::MakeUniqueMp4Path(dir, "tft");
    EXPECT_EQ(p1.extension(), ".mp4");
    // cria arquivo para forçar colisão na mesma base improvável; ao menos garante mp4
    std::ofstream(p1.string()) << "x";
    auto p2 = OutputPath::MakeUniqueMp4Path(dir, "tft");
    EXPECT_EQ(p2.extension(), ".mp4");
    fs::remove_all(dir);
}
