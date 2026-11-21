#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "rounds/IRoundDetector.h"
#include "rounds/RoundDetector.h"
#include "rounds/RoundDetectorConfigLoader.h"

using tft::DefaultRoundDetectorConfig;
using tft::DetectedRound;
using tft::FrameView;
using tft::NormalizedRect;
using tft::ParseRoundDetectorConfigJson;
using tft::RoundDetector;
using tft::RoundDetectorConfig;
using tft::RoundDetectorConfigSampleJson;

namespace {

struct SolidFrame {
    explicit SolidFrame(std::uint8_t luma, int width = 100, int height = 100)
        : pixels(static_cast<std::size_t>(width) * height * 4, luma),
          w(width), h(height) {}

    FrameView View(std::int64_t timestamp_100ns) const {
        FrameView frame{};
        frame.bgra = pixels.data();
        frame.width = w;
        frame.height = h;
        frame.stride = w * 4;
        frame.timestamp_100ns = timestamp_100ns;
        return frame;
    }

    std::vector<std::uint8_t> pixels;
    int w;
    int h;
};

constexpr std::int64_t Ms(int milliseconds) {
    return static_cast<std::int64_t>(milliseconds) * 10'000; // 1 ms = 10.000 ticks de 100 ns
}

RoundDetectorConfig TestConfig() {
    RoundDetectorConfig config;
    config.roi = NormalizedRect{0.0, 0.0, 1.0, 1.0};
    config.labels = {"1-1", "1-2"};
    config.change_threshold = 10;
    config.stability_frames = 2;
    config.cooldown_ms = 5;
    config.warmup_ms = 0;
    config.emit_initial_round = false;
    config.source = "test-roi";
    return config;
}

} // namespace

TEST(RoundDetectorTests, SignatureReadsOnlyTheConfiguredRoi) {
    SolidFrame frame(0, 8, 4);
    for (int y = 1; y < 3; ++y) {
        for (int x = 4; x < 6; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * 8 + x) * 4;
            frame.pixels[offset + 0] = 255;
            frame.pixels[offset + 1] = 255;
            frame.pixels[offset + 2] = 255;
        }
    }

    const auto signature = RoundDetector::Signature(
        frame.View(0), NormalizedRect{0.5, 0.25, 0.25, 0.5});
    for (const auto value : signature) EXPECT_EQ(255, value);
}

TEST(RoundDetectorTests, MeanAbsoluteDifferenceAveragesSignedDifferences) {
    RoundDetector::SignatureArray a{};
    RoundDetector::SignatureArray b{};
    a.fill(0);
    for (std::size_t i = 0; i < b.size(); i += 2) b[i] = 100;
    EXPECT_EQ(50, RoundDetector::MeanAbsoluteDifference(a, b));
}

TEST(RoundDetectorTests, ReportsStableRoiChangeAtItsFirstFrame) {
    RoundDetector detector(TestConfig());
    const SolidFrame dark(40);
    const SolidFrame bright(200);

    for (int i = 0; i < 5; ++i) detector.ProcessFrame(dark.View(Ms(i)));
    EXPECT_TRUE(detector.DrainDetections().empty());

    for (int i = 0; i < 3; ++i) detector.ProcessFrame(bright.View(Ms(5 + i)));

    const auto rounds = detector.DrainDetections();
    ASSERT_EQ(1u, rounds.size());
    EXPECT_EQ("1-1", rounds[0].label);
    EXPECT_EQ(50'000, rounds[0].timestamp_100ns);
    EXPECT_EQ("test-roi", rounds[0].source);
    EXPECT_GT(rounds[0].confidence, 0.0);
    EXPECT_TRUE(detector.DrainDetections().empty());
}

TEST(RoundDetectorTests, IgnoresSingleFrameFlicker) {
    RoundDetector detector(TestConfig());
    const SolidFrame dark(40);
    const SolidFrame bright(200);

    for (int i = 0; i < 5; ++i) detector.ProcessFrame(dark.View(Ms(i)));
    detector.ProcessFrame(bright.View(Ms(5)));
    for (int i = 0; i < 3; ++i) detector.ProcessFrame(dark.View(Ms(6 + i)));

    EXPECT_TRUE(detector.DrainDetections().empty());
}

TEST(RoundDetectorTests, CooldownKeepsTheOriginalChangeTimestamp) {
    RoundDetectorConfig config = TestConfig();
    config.cooldown_ms = 5;
    RoundDetector detector(config);
    const SolidFrame dark(40);
    const SolidFrame bright(200);

    for (int i = 0; i < 5; ++i) detector.ProcessFrame(dark.View(Ms(i)));
    for (int i = 0; i < 2; ++i) detector.ProcessFrame(bright.View(Ms(5 + i)));
    // A segunda virada começa em 7 ms, mas só é confirmada depois do cooldown.
    for (int i = 7; i <= 12; ++i) detector.ProcessFrame(dark.View(Ms(i)));

    const auto rounds = detector.DrainDetections();
    ASSERT_EQ(2u, rounds.size());
    EXPECT_EQ("1-1", rounds[0].label);
    EXPECT_EQ(50'000, rounds[0].timestamp_100ns);
    EXPECT_EQ("1-2", rounds[1].label);
    EXPECT_EQ(70'000, rounds[1].timestamp_100ns);
}

TEST(RoundDetectorTests, ResetRestartsLabelsAndPendingDetections) {
    RoundDetector detector(TestConfig());
    const SolidFrame dark(40);
    const SolidFrame bright(200);

    for (int i = 0; i < 5; ++i) detector.ProcessFrame(dark.View(Ms(i)));
    for (int i = 0; i < 2; ++i) detector.ProcessFrame(bright.View(Ms(5 + i)));
    ASSERT_EQ(1u, detector.DrainDetections().size());

    detector.Reset();
    EXPECT_TRUE(detector.DrainDetections().empty());

    for (int i = 0; i < 5; ++i) detector.ProcessFrame(dark.View(Ms(i)));
    for (int i = 0; i < 2; ++i) detector.ProcessFrame(bright.View(Ms(5 + i)));
    const auto restarted = detector.DrainDetections();
    ASSERT_EQ(1u, restarted.size());
    EXPECT_EQ("1-1", restarted[0].label);
}

TEST(RoundDetectorTests, EmitsInitialRoundThenLabelsFollowingChanges) {
    RoundDetectorConfig config = TestConfig();
    config.emit_initial_round = true;
    config.cooldown_ms = 0;
    RoundDetector detector(config);
    const SolidFrame dark(40);
    const SolidFrame bright(200);

    detector.ProcessFrame(dark.View(Ms(0)));
    detector.ProcessFrame(dark.View(Ms(1)));
    detector.ProcessFrame(bright.View(Ms(2)));
    detector.ProcessFrame(bright.View(Ms(3)));

    const auto rounds = detector.DrainDetections();
    ASSERT_EQ(2u, rounds.size());
    EXPECT_EQ("1-1", rounds[0].label);
    EXPECT_EQ(0, rounds[0].timestamp_100ns);
    EXPECT_EQ("1-2", rounds[1].label);
    EXPECT_EQ(20'000, rounds[1].timestamp_100ns);
}

TEST(RoundDetectorTests, ConfigSampleParsesWithDefaultsPreserved) {
    RoundDetectorConfig config = DefaultRoundDetectorConfig();
    std::string error;
    ASSERT_TRUE(ParseRoundDetectorConfigJson(RoundDetectorConfigSampleJson(), config, &error))
        << error;
    EXPECT_DOUBLE_EQ(0.42, config.roi.x);
    EXPECT_DOUBLE_EQ(0.08, config.roi.h);
    EXPECT_EQ("hud-counter", config.source);
    EXPECT_EQ(2500, config.cooldown_ms);
    EXPECT_TRUE(config.emit_initial_round);
    EXPECT_FALSE(config.labels.empty());
    EXPECT_EQ("1-1", config.labels.front());
}

TEST(RoundDetectorTests, ConfigParserAcceptsBooleanAndTextForInitialRound) {
    RoundDetectorConfig config = DefaultRoundDetectorConfig();
    ASSERT_TRUE(ParseRoundDetectorConfigJson(R"({"emit_initial_round":false})", config, nullptr));
    EXPECT_FALSE(config.emit_initial_round);
    ASSERT_TRUE(ParseRoundDetectorConfigJson(R"({"emit_initial_round":"sim"})", config, nullptr));
    EXPECT_TRUE(config.emit_initial_round);

    std::string error;
    EXPECT_FALSE(ParseRoundDetectorConfigJson(R"({"emit_initial_round":"talvez"})", config, &error));
    EXPECT_FALSE(error.empty());
}

TEST(RoundDetectorTests, ConfigParserRejectsOutOfBoundsRoi) {
    RoundDetectorConfig config = DefaultRoundDetectorConfig();
    std::string error;
    EXPECT_FALSE(ParseRoundDetectorConfigJson(R"({"roi":"0.9,0.9,0.2,0.2"})", config, &error));
    EXPECT_FALSE(error.empty());
}

TEST(RoundDetectorTests, ConfigParserRejectsEmptyLabels) {
    RoundDetectorConfig config = DefaultRoundDetectorConfig();
    std::string error;
    EXPECT_FALSE(ParseRoundDetectorConfigJson(R"({"labels":"  , , "})", config, &error));
    EXPECT_FALSE(error.empty());
}

TEST(RoundDetectorTests, ConfigParserKeepsFieldsAbsentFromJson) {
    RoundDetectorConfig config = DefaultRoundDetectorConfig();
    config.source = "custom";
    config.stability_frames = 7;
    ASSERT_TRUE(ParseRoundDetectorConfigJson(R"({"cooldown_ms":"4000"})", config, nullptr));
    EXPECT_EQ(4000, config.cooldown_ms);
    EXPECT_EQ("custom", config.source);
    EXPECT_EQ(7, config.stability_frames);
}
