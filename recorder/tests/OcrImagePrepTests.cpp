#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "rounds/OcrImagePrep.h"
#include "rounds/OcrTransition.h"

using tft::ImageRect;
using tft::MakeBgraSignature;
using tft::OcrSignatureSample;
using tft::PrepareOcrBitmap;
using tft::FindOcrTransition;

namespace {

std::vector<std::uint8_t> SolidBgra(int w, int h, std::uint8_t gray) {
    return std::vector<std::uint8_t>(static_cast<std::size_t>(w) * h * 4, gray);
}

} // namespace

TEST(OcrImagePrepTests, ConvertsGrayContrastAndScales) {
    // Preto e branco com contraste 200 afasta do cinza neutro (128).
    std::vector<std::uint8_t> src(4 * 4 * 4, 0);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            auto* p = src.data() + (static_cast<std::size_t>(y) * 4 + x) * 4;
            const std::uint8_t v = x < 2 ? 0 : 255;
            p[0] = p[1] = p[2] = v;
            p[3] = 255;
        }
    }
    std::vector<std::uint8_t> out;
    int w = 0;
    int h = 0;
    ASSERT_TRUE(PrepareOcrBitmap(src.data(), 4, 4, 16, nullptr, 2, 200,
                                 out, w, h));
    EXPECT_EQ(8, w);
    EXPECT_EQ(8, h);
    EXPECT_EQ(8u * 8u * 4u, out.size());
    // Com contraste 200, preto vira 0 e branco vira 255 (clamp).
    EXPECT_EQ(0, out[0]);
    EXPECT_EQ(255, out[(0 * 8 + 4) * 4]);
}

TEST(OcrImagePrepTests, CropsRectAndRejectsInvalidInput) {
    auto src = SolidBgra(10, 10, 128);
    std::vector<std::uint8_t> out;
    int w = 0;
    int h = 0;
    const ImageRect rect{2, 3, 4, 2};
    ASSERT_TRUE(PrepareOcrBitmap(src.data(), 10, 10, 40, &rect, 1, 100,
                                 out, w, h));
    EXPECT_EQ(4, w);
    EXPECT_EQ(2, h);

    EXPECT_FALSE(PrepareOcrBitmap(nullptr, 10, 10, 40, nullptr, 1, 100,
                                  out, w, h));
    EXPECT_FALSE(PrepareOcrBitmap(src.data(), 10, 10, 40, nullptr, 5, 100,
                                  out, w, h));
    const ImageRect outside{20, 20, 4, 2};
    EXPECT_FALSE(PrepareOcrBitmap(src.data(), 10, 10, 40, &outside, 1, 100,
                                  out, w, h));
}

TEST(OcrImagePrepTests, SignatureCapturesAverageBrightness) {
    auto dark = SolidBgra(8, 8, 40);
    auto bright = SolidBgra(8, 8, 200);
    const auto sd = MakeBgraSignature(dark.data(), 8, 8, 32, 2, 2);
    const auto sb = MakeBgraSignature(bright.data(), 8, 8, 32, 2, 2);
    ASSERT_EQ(4u, sd.size());
    EXPECT_EQ(sd, std::vector<std::uint8_t>(4, 40));
    EXPECT_EQ(sb, std::vector<std::uint8_t>(4, 200));
}

TEST(OcrTransitionTests, FindsVisualChangeBeforeFirstRead) {
    std::vector<OcrSignatureSample> history;
    for (int i = 0; i < 12; ++i) {
        OcrSignatureSample s;
        s.timestamp_100ns = (100 + i * 100) * 10'000; // 100..1200 ms
        s.signature = std::vector<std::uint8_t>(32 * 8, i < 6 ? 40 : 200);
        history.push_back(std::move(s));
    }
    const auto first_read = 1500LL * 10'000;
    const auto timestamp = FindOcrTransition(history, first_read,
                                             20'000LL * 10'000, 12);
    // A virada acontece na amostra de 700 ms (i == 6).
    EXPECT_EQ(700LL * 10'000, timestamp);
}

TEST(OcrTransitionTests, ReturnsZeroWhenNoConfidentChange) {
    std::vector<OcrSignatureSample> history;
    for (int i = 0; i < 12; ++i) {
        OcrSignatureSample s;
        s.timestamp_100ns = (100 + i * 100) * 10'000;
        s.signature = std::vector<std::uint8_t>(32 * 8, 128);
        history.push_back(std::move(s));
    }
    EXPECT_EQ(0, FindOcrTransition(history, 1500LL * 10'000,
                                   20'000LL * 10'000, 12));
}
