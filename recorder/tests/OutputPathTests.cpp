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

TEST(OutputPath, ValidateDirectorySyntaxAcceptsAbsolutePaths) {
    std::string error;
    EXPECT_TRUE(OutputPath::ValidateDirectorySyntax("C:/vods", &error)) << error;
    error.clear();
    EXPECT_TRUE(OutputPath::ValidateDirectorySyntax("C:\\vods", &error)) << error;
    error.clear();
    EXPECT_TRUE(OutputPath::ValidateDirectorySyntax("/tmp/vods", &error)) << error;
    error.clear();
    EXPECT_TRUE(OutputPath::ValidateDirectorySyntax("//server/share/vods", &error)) << error;
}

TEST(OutputPath, ValidateDirectorySyntaxRejectsUnsafePaths) {
    struct Case { const char* path; const char* message; };
    const Case cases[] = {
        {"vods", "absoluto"},
        {"C:", "absoluto"},
        {"C:vods", "absoluto"},
        {"C:/vods/../out", "não são permitidos"},
        {"C:/vo:ds", "caractere inválido"},
        {"C:/CON/vods", "dispositivo reservado"},
        {"C:/vods/NUL.txt", "dispositivo reservado"},
        {"C:/vods/out.", "termina com '.'"},
        {"C:/vods/out ", "espaço"},
        {"\\\\server", "UNC incompleto"}
    };
    for (const auto& item : cases) {
        std::string error;
        EXPECT_FALSE(OutputPath::ValidateDirectorySyntax(item.path, &error)) << item.path;
        EXPECT_NE(error.find(item.message), std::string::npos) << item.path << " => " << error;
    }
}

TEST(OutputPath, EnsureDirectoryRejectsInvalidSyntaxAndExistingFile) {
    std::string error;
    EXPECT_FALSE(OutputPath::EnsureDirectory("vods", &error));
    EXPECT_NE(error.find("absoluto"), std::string::npos) << error;

    auto dir = fs::temp_directory_path() / "tft_vod_recorder_path_validation";
    fs::remove_all(dir);
    fs::create_directories(dir);
    auto file = dir / "file.mp4";
    std::ofstream(file) << "x";

    error.clear();
    EXPECT_FALSE(OutputPath::EnsureDirectory(file, &error));
    EXPECT_NE(error.find("não é um diretório"), std::string::npos) << error;
    fs::remove_all(dir);
}

TEST(OutputPath, SanitizeFilenameHandlesReservedNamesAndTrailingDots) {
    EXPECT_EQ(OutputPath::SanitizeFilename("CON.txt"), "_CON.txt");
    EXPECT_EQ(OutputPath::SanitizeFilename("vods. "), "vods");
}
