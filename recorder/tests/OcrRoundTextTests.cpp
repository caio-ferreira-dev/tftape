#include <gtest/gtest.h>

#include <string>

#include "rounds/OcrRoundDetectorConfigLoader.h"
#include "rounds/OcrRoundText.h"

using tft::DefaultOcrRoundDetectorConfig;
using tft::NormalizeOcrRoundText;
using tft::OcrRoundDetectorConfig;
using tft::OcrRoundDetectorConfigSampleJson;
using tft::ParseExactRoundLabel;
using tft::ParseOcrRoundDetectorConfigJson;
using tft::TryParseRoundLabel;

TEST(OcrRoundTextTests, NormalizesHyphenVariantsAndOcrConfusions) {
    EXPECT_EQ("1-1", NormalizeOcrRoundText("1 – 1"));   // en dash
    EXPECT_EQ("1-1", NormalizeOcrRoundText("1—1"));     // em dash
    EXPECT_EQ("1-1", NormalizeOcrRoundText("l-l"));     // confusão l/L -> 1
    EXPECT_EQ("1-1", NormalizeOcrRoundText("I-I"));
    EXPECT_EQ("1-1", NormalizeOcrRoundText("ROUND 1 - 1")); // letras descartadas
}

TEST(OcrRoundTextTests, ParsesRoundLabelInsideRecognizedText) {
    std::string label;
    ASSERT_TRUE(TryParseRoundLabel("ROUND 1 - 1", label));
    EXPECT_EQ("1-1", label);

    ASSERT_TRUE(TryParseRoundLabel("stage 2-3 x", label));
    EXPECT_EQ("2-3", label);

    ASSERT_TRUE(TryParseRoundLabel("6 – 7", label));
    EXPECT_EQ("6-7", label);
}

TEST(OcrRoundTextTests, ExactParseRejectsSurroundingText) {
    std::string label;
    EXPECT_TRUE(ParseExactRoundLabel("1 - 1", label));
    EXPECT_EQ("1-1", label);
    EXPECT_FALSE(ParseExactRoundLabel("ROUND 1 - 1", label));
    EXPECT_FALSE(ParseExactRoundLabel("foo 1-1 bar", label));
}

TEST(OcrRoundTextTests, RejectsOutOfRangeAndMalformedLabels) {
    std::string label;
    EXPECT_FALSE(TryParseRoundLabel("0-1", label));
    EXPECT_FALSE(TryParseRoundLabel("1-0", label));
    EXPECT_FALSE(TryParseRoundLabel("10-1", label));
    EXPECT_FALSE(TryParseRoundLabel("abc", label));
    EXPECT_FALSE(TryParseRoundLabel("1-", label));
}

TEST(OcrRoundTextTests, RecoversCounterFromDirtyOcrLines) {
    std::string label;

    // Casos reais do log do recorder: o OCR cola o contador a outros números.
    ASSERT_TRUE(TryParseRoundLabel("& 1-1 0", label));
    EXPECT_EQ("1-1", label);

    ASSERT_TRUE(TryParseRoundLabel("@ 27 | 1-2", label));
    EXPECT_EQ("1-2", label);

    ASSERT_TRUE(TryParseRoundLabel("& 1-4 | @ 14 A", label));
    EXPECT_EQ("1-4", label);

    // "1-10" é "1-1" colado a um zero vizinho; rounds só vão até 9.
    ASSERT_TRUE(TryParseRoundLabel("1-10", label));
    EXPECT_EQ("1-1", label);
}

TEST(OcrRoundTextTests, SampleConfigParsesWithDefaults) {
    OcrRoundDetectorConfig config = DefaultOcrRoundDetectorConfig();
    ASSERT_TRUE(ParseOcrRoundDetectorConfigJson(OcrRoundDetectorConfigSampleJson(), config));

    EXPECT_DOUBLE_EQ(0.15, config.band.x);
    EXPECT_DOUBLE_EQ(0.0, config.band.y);
    EXPECT_DOUBLE_EQ(0.70, config.band.w);
    EXPECT_DOUBLE_EQ(0.18, config.band.h);
    EXPECT_EQ(100, config.sample_interval_ms);
    EXPECT_EQ(500, config.warmup_ms);
    EXPECT_EQ(2, config.confirmation_samples);
    EXPECT_EQ(60'000, config.confirmation_window_ms);
    EXPECT_EQ(1200, config.cooldown_ms);
    EXPECT_EQ("hud-ocr", config.source);
}

TEST(OcrRoundTextTests, ConfigParserRejectsInvalidBand) {
    OcrRoundDetectorConfig config = DefaultOcrRoundDetectorConfig();
    EXPECT_FALSE(ParseOcrRoundDetectorConfigJson(
        R"({"band":"0.2,0.1,1.5,0.2"})", config));
    EXPECT_FALSE(ParseOcrRoundDetectorConfigJson(
        R"({"band":"0.2,0.1,0.3"})", config));
}

TEST(OcrRoundTextTests, ConfigParserKeepsFieldsAbsentFromJson) {
    OcrRoundDetectorConfig config = DefaultOcrRoundDetectorConfig();
    config.sample_interval_ms = 333;
    ASSERT_TRUE(ParseOcrRoundDetectorConfigJson(R"({"source":"hud-ocr-test"})", config));
    EXPECT_EQ(333, config.sample_interval_ms);
    EXPECT_EQ("hud-ocr-test", config.source);
}
