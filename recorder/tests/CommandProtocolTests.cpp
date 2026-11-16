#include <gtest/gtest.h>

#include "app/CommandProtocol.h"

using tft::CommandProtocol;
using tft::CommandType;

TEST(CommandProtocol, ParsesStartCommandWithOptions) {
    auto c = CommandProtocol::ParseLine(
        "{\"cmd\":\"start\",\"output_dir\":\"C:/vods\",\"resolution\":\"1080p\",\"fps\":\"60\",\"system_audio\":true,\"microphone\":false}");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->type, CommandType::Start);
    EXPECT_EQ(c->settings.output_dir, "C:/vods");
    EXPECT_EQ(tft::ToString(c->settings.resolution), "1080p");
    EXPECT_EQ(tft::ToString(c->settings.framerate), "60");
    EXPECT_TRUE(c->settings.audio.system_audio);
    EXPECT_FALSE(c->settings.audio.microphone);
}

TEST(CommandProtocol, ParsesStopPingShutdown) {
    auto stop = CommandProtocol::ParseLine("{\"cmd\":\"stop\"}");
    ASSERT_TRUE(stop.has_value());
    EXPECT_EQ(stop->type, CommandType::Stop);
    auto ping = CommandProtocol::ParseLine("{\"cmd\":\"ping\"}");
    ASSERT_TRUE(ping.has_value());
    EXPECT_EQ(ping->type, CommandType::Ping);
}

TEST(CommandProtocol, EventSerializesJsonLine) {
    auto e = CommandProtocol::Ok("recording_started", "ok", "C:/vods/a.mp4");
    auto j = e.ToJsonLine();
    EXPECT_NE(j.find("\"event\":\"recording_started\""), std::string::npos);
    EXPECT_NE(j.find("\"ok\":true"), std::string::npos);
    EXPECT_NE(j.find("a.mp4"), std::string::npos);
}
