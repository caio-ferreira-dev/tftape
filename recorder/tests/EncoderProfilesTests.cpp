#include <gtest/gtest.h>

#include "encode/AudioEncoderMf.h"
#include "encode/VideoEncoderMf.h"
#include "output/RecordingSettings.h"

using tft::AudioEncoderConfig;
using tft::BuildAudioEncoderConfig;
using tft::BuildVideoEncoderConfig;
using tft::Framerate;
using tft::RecordingSettings;
using tft::Resolution;
using tft::VideoEncoderConfig;

namespace {

VideoEncoderConfig ConfigFor(Resolution resolution, Framerate framerate) {
    RecordingSettings settings;
    settings.output_dir = "C:/vods";
    settings.resolution = resolution;
    settings.framerate = framerate;
    return BuildVideoEncoderConfig(settings.ToProfile());
}

} // namespace

TEST(EncoderProfiles, VideoConfigMatches720p30Profile) {
    auto config = ConfigFor(Resolution::P720, Framerate::F30);
    EXPECT_EQ(config.width, 1280);
    EXPECT_EQ(config.height, 720);
    EXPECT_EQ(config.fps, 30);
    EXPECT_EQ(config.bitrate, 4'000'000);
    EXPECT_EQ(config.frame_duration_100ns, 333'333);
    EXPECT_EQ(config.input_subtype, "NV12");
    EXPECT_EQ(config.output_subtype, "H264");
}

TEST(EncoderProfiles, VideoConfigMatches720p60Profile) {
    auto config = ConfigFor(Resolution::P720, Framerate::F60);
    EXPECT_EQ(config.width, 1280);
    EXPECT_EQ(config.height, 720);
    EXPECT_EQ(config.fps, 60);
    EXPECT_EQ(config.bitrate, 6'000'000);
    EXPECT_EQ(config.frame_duration_100ns, 166'666);
}

TEST(EncoderProfiles, VideoConfigMatches1080pProfiles) {
    auto p30 = ConfigFor(Resolution::P1080, Framerate::F30);
    EXPECT_EQ(p30.width, 1920);
    EXPECT_EQ(p30.height, 1080);
    EXPECT_EQ(p30.fps, 30);
    EXPECT_EQ(p30.bitrate, 8'000'000);

    auto p60 = ConfigFor(Resolution::P1080, Framerate::F60);
    EXPECT_EQ(p60.width, 1920);
    EXPECT_EQ(p60.height, 1080);
    EXPECT_EQ(p60.fps, 60);
    EXPECT_EQ(p60.bitrate, 12'000'000);
    EXPECT_EQ(p60.frame_duration_100ns, 166'666);
}

TEST(EncoderProfiles, AudioConfigMapsPcm16ToAac) {
    AudioEncoderConfig config = BuildAudioEncoderConfig(48000, 2, 160000);
    EXPECT_EQ(config.sample_rate, 48000);
    EXPECT_EQ(config.channels, 2);
    EXPECT_EQ(config.bits_per_sample, 16);
    EXPECT_EQ(config.bitrate, 160000);
    EXPECT_EQ(config.block_align, 4);
    EXPECT_EQ(config.avg_bytes_per_second, 192000);
    EXPECT_EQ(config.input_subtype, "PCM");
    EXPECT_EQ(config.output_subtype, "AAC");
}
