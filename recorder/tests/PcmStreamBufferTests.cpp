#include <gtest/gtest.h>

#include "audio/PcmStreamBuffer.h"

using namespace tft;

TEST(PcmStreamBufferTests, AppendAndReadPreservesOrder) {
    PcmStreamBuffer buffer(64);
    const uint8_t input[] = {1, 2, 3, 4};
    buffer.Append(input, sizeof(input));

    uint8_t output[8]{};
    EXPECT_EQ(buffer.Read(output, sizeof(output)), 4u);
    EXPECT_EQ(output[0], 1);
    EXPECT_EQ(output[1], 2);
    EXPECT_EQ(output[2], 3);
    EXPECT_EQ(output[3], 4);
    EXPECT_TRUE(buffer.Empty());
}

TEST(PcmStreamBufferTests, PartialReadKeepsRemainder) {
    PcmStreamBuffer buffer(64);
    const uint8_t input[] = {1, 2, 3, 4, 5, 6};
    buffer.Append(input, sizeof(input));

    uint8_t first[2]{};
    EXPECT_EQ(buffer.Read(first, sizeof(first)), 2u);
    EXPECT_EQ(buffer.Size(), 4u);
    EXPECT_EQ(first[0], 1);
    EXPECT_EQ(first[1], 2);

    uint8_t rest[8]{};
    EXPECT_EQ(buffer.Read(rest, sizeof(rest)), 4u);
    EXPECT_EQ(rest[0], 3);
    EXPECT_EQ(rest[3], 6);
    EXPECT_TRUE(buffer.Empty());
}

TEST(PcmStreamBufferTests, AppendAfterPartialReadKeepsOrder) {
    PcmStreamBuffer buffer(64);
    const uint8_t first[] = {1, 2, 3};
    buffer.Append(first, sizeof(first));

    uint8_t out[2]{};
    ASSERT_EQ(buffer.Read(out, sizeof(out)), 2u);

    const uint8_t second[] = {4, 5};
    buffer.Append(second, sizeof(second));

    uint8_t rest[8]{};
    EXPECT_EQ(buffer.Read(rest, sizeof(rest)), 3u);
    EXPECT_EQ(rest[0], 3);
    EXPECT_EQ(rest[1], 4);
    EXPECT_EQ(rest[2], 5);
}

TEST(PcmStreamBufferTests, TrimsOldestDataWhenExceedingCapacity) {
    PcmStreamBuffer buffer(4);
    const uint8_t first[] = {1, 2, 3};
    const uint8_t second[] = {4, 5, 6};
    buffer.Append(first, sizeof(first));
    buffer.Append(second, sizeof(second));

    uint8_t out[8]{};
    EXPECT_EQ(buffer.Read(out, sizeof(out)), 4u);
    EXPECT_EQ(out[0], 3);
    EXPECT_EQ(out[1], 4);
    EXPECT_EQ(out[2], 5);
    EXPECT_EQ(out[3], 6);
}

TEST(PcmStreamBufferTests, SetCapacityDropsOldestData) {
    PcmStreamBuffer buffer(8);
    const uint8_t input[] = {1, 2, 3, 4, 5, 6};
    buffer.Append(input, sizeof(input));

    buffer.SetCapacity(4);
    EXPECT_EQ(buffer.Size(), 4u);

    uint8_t out[8]{};
    EXPECT_EQ(buffer.Read(out, sizeof(out)), 4u);
    EXPECT_EQ(out[0], 3);
    EXPECT_EQ(out[3], 6);
}

TEST(PcmStreamBufferTests, ReadZeroBytesReturnsZero) {
    PcmStreamBuffer buffer(16);
    const uint8_t input[] = {1, 2};
    buffer.Append(input, sizeof(input));
    uint8_t out[2]{};
    EXPECT_EQ(buffer.Read(out, 0), 0u);
    EXPECT_EQ(buffer.Size(), 2u);
}

TEST(PcmStreamBufferTests, ClearRemovesAllData) {
    PcmStreamBuffer buffer(16);
    const uint8_t input[] = {1, 2, 3};
    buffer.Append(input, sizeof(input));
    buffer.Clear();
    EXPECT_TRUE(buffer.Empty());
    uint8_t out[4]{};
    EXPECT_EQ(buffer.Read(out, sizeof(out)), 0u);
}

TEST(PcmStreamBufferTests, AppendNullIsIgnored) {
    PcmStreamBuffer buffer(16);
    buffer.Append(nullptr, 4);
    EXPECT_TRUE(buffer.Empty());
}
