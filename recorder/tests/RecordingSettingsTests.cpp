#include <gtest/gtest.h>

#include "output/RecordingSettings.h"

using tft::Framerate;
using tft::ParseFramerate;
using tft::ParseResolution;
using tft::RecordingSettings;
using tft::Resolution;

TEST(RecordingSettings, ParsesResolutionAndFramerate) {
    Resolution r; Framerate f;
    EXPECT_TRUE(ParseResolution("720p", r)); EXPECT_EQ(r, Resolution::P720);
    EXPECT_TRUE(ParseResolution("1080", r)); EXPECT_EQ(r, Resolution::P1080);
    EXPECT_FALSE(ParseResolution("480p", r));
    EXPECT_TRUE(ParseFramerate("30", f)); EXPECT_EQ(f, Framerate::F30);
    EXPECT_TRUE(ParseFramerate("60fps", f)); EXPECT_EQ(f, Framerate::F60);
    EXPECT_FALSE(ParseFramerate("24", f));
}

TEST(RecordingSettings, ProfilesMatchSelections) {
    RecordingSettings s;
    s.output_dir = "C:/tmp";
    s.resolution = Resolution::P1080;
    s.framerate = Framerate::F60;
    auto p = s.ToProfile();
    EXPECT_EQ(p.width, 1920);
    EXPECT_EQ(p.height, 1080);
    EXPECT_EQ(p.fps, 60);

    s.resolution = Resolution::P720;
    s.framerate = Framerate::F30;
    p = s.ToProfile();
    EXPECT_EQ(p.width, 1280);
    EXPECT_EQ(p.height, 720);
    EXPECT_EQ(p.fps, 30);
}

TEST(RecordingSettings, RequiresOutputDir) {
    RecordingSettings s;
    std::string err;
    EXPECT_FALSE(s.IsValid(&err));
    s.output_dir = "C:/videos";
    EXPECT_TRUE(s.IsValid(&err));
}
