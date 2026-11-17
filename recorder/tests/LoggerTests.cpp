#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "util/Clock.h"
#include "util/Logger.h"

using tft::FakeClock;

namespace {

std::filesystem::path TempLogDir(const std::string& suffix) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
        ("tft_logger_test_" + std::to_string(stamp) + "_" + suffix);
}

std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

TEST(LoggerTests, WritesTimestampedLineToConfiguredFile) {
    const auto dir = TempLogDir("single");
    FakeClock clock(std::chrono::system_clock::from_time_t(1700000000));
    tft::Logger logger(clock);

    tft::Logger::Options options;
    options.directory = dir.string();
    options.max_bytes = 64 * 1024;
    options.echo_stderr = false;
    ASSERT_TRUE(logger.Configure(options));

    logger.Info("mensagem de teste");
    const auto content = ReadAll(logger.FilePath());
    EXPECT_NE(content.find("[INFO] mensagem de teste"), std::string::npos);
    EXPECT_NE(content.find("2023-11-"), std::string::npos); // fake clock: 2023-11-14 UTC
    EXPECT_EQ(content.back(), '\n');

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST(LoggerTests, RotatesFileKeepingConfiguredCount) {
    const auto dir = TempLogDir("rotate");
    FakeClock clock(std::chrono::system_clock::from_time_t(1700000000));
    tft::Logger logger(clock);

    tft::Logger::Options options;
    options.directory = dir.string();
    options.max_bytes = 96;
    options.max_files = 2;
    options.echo_stderr = false;
    ASSERT_TRUE(logger.Configure(options));

    for (int i = 0; i < 40; ++i) {
        logger.Info("linha de diagnóstico com texto suficiente para estourar o limite " + std::to_string(i));
    }

    std::error_code ec;
    EXPECT_TRUE(std::filesystem::exists(dir / "recorder.log", ec));
    EXPECT_TRUE(std::filesystem::exists(dir / "recorder.log.1", ec));
    EXPECT_TRUE(std::filesystem::exists(dir / "recorder.log.2", ec));
    EXPECT_FALSE(std::filesystem::exists(dir / "recorder.log.3", ec));
    EXPECT_NE(ReadAll(dir / "recorder.log").find("[INFO]"), std::string::npos);

    std::filesystem::remove_all(dir, ec);
}

TEST(LoggerTests, FakeClockAdvanceChangesTimestamp) {
    const auto dir = TempLogDir("clock");
    FakeClock clock(std::chrono::system_clock::from_time_t(1700000000));
    tft::Logger logger(clock);

    tft::Logger::Options options;
    options.directory = dir.string();
    options.echo_stderr = false;
    ASSERT_TRUE(logger.Configure(options));

    logger.Info("antes");
    clock.Advance(std::chrono::seconds(2));
    logger.Info("depois");

    const auto content = ReadAll(logger.FilePath());
    std::istringstream lines(content);
    std::string first, second;
    ASSERT_TRUE(static_cast<bool>(std::getline(lines, first)));
    ASSERT_TRUE(static_cast<bool>(std::getline(lines, second)));
    ASSERT_GE(first.size(), 24u);
    ASSERT_GE(second.size(), 24u);
    EXPECT_NE(first.substr(1, 19), second.substr(1, 19));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
