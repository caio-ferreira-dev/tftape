#include <gtest/gtest.h>

#include <string>

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

TEST(CommandProtocol, UsesDocumentedDefaultsWhenOptionalFieldsAreAbsent) {
    auto c = CommandProtocol::ParseLine("{\"cmd\":\"start\",\"output_dir\":\"C:/vods\"}");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(tft::ToString(c->settings.resolution), "720p");
    EXPECT_EQ(tft::ToString(c->settings.framerate), "30");
    EXPECT_TRUE(c->settings.audio.game_audio);
    EXPECT_FALSE(c->settings.audio.system_audio);
    EXPECT_FALSE(c->settings.audio.microphone);
}

TEST(CommandProtocol, ParsesStopPingShutdown) {
    auto stop = CommandProtocol::ParseLine("{\"cmd\":\"stop\"}");
    ASSERT_TRUE(stop.has_value());
    EXPECT_EQ(stop->type, CommandType::Stop);
    auto ping = CommandProtocol::ParseLine("{\"cmd\":\"ping\"}");
    ASSERT_TRUE(ping.has_value());
    EXPECT_EQ(ping->type, CommandType::Ping);
    auto shutdown = CommandProtocol::ParseLine("{\"cmd\":\"shutdown\"}");
    ASSERT_TRUE(shutdown.has_value());
    EXPECT_EQ(shutdown->type, CommandType::Shutdown);
}

TEST(CommandProtocol, AcceptsWhitespaceAndCrLf) {
    auto c = CommandProtocol::ParseLine("  {\"cmd\":\"ping\"}\r\n");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->type, CommandType::Ping);
}

TEST(CommandProtocol, EventSerializesJsonLine) {
    auto e = CommandProtocol::Ok("recording_started", "ok", "C:/vods/a.mp4");
    auto j = e.ToJsonLine();
    EXPECT_NE(j.find("\"event\":\"recording_started\""), std::string::npos);
    EXPECT_NE(j.find("\"ok\":true"), std::string::npos);
    EXPECT_NE(j.find("a.mp4"), std::string::npos);
}

TEST(CommandProtocol, RejectsMalformedJson) {
    const char* invalid[] = {
        "{\"cmd\":\"start\"",                    // falta '}'
        "{'cmd':'start'}",                       // aspas simples
        "{\"cmd\":\"start\",}",                  // vírgula final
        "not json",                              // não é objeto
        "{\"cmd\":\"start\"} lixo",              // texto após o objeto
        "{\"cmd\":\"start\" \"fps\":\"30\"}"     // falta ','
    };
    for (const char* line : invalid) {
        std::string error;
        EXPECT_FALSE(CommandProtocol::ParseLine(line, &error).has_value()) << line;
        EXPECT_NE(error.find("JSON inválido"), std::string::npos) << line << " => " << error;
    }
}

TEST(CommandProtocol, RejectsWrongFieldTypes) {
    const char* invalid[] = {
        "{\"cmd\":1}",
        "{\"cmd\":\"start\",\"output_dir\":null}",
        "{\"cmd\":\"start\",\"game_audio\":\"true\"}",
        "{\"cmd\":\"start\",\"fps\":30}",
        "{\"cmd\":\"start\",\"resolution\":false}"
    };
    for (const char* line : invalid) {
        std::string error;
        EXPECT_FALSE(CommandProtocol::ParseLine(line, &error).has_value()) << line;
        EXPECT_FALSE(error.empty()) << line;
    }
}

TEST(CommandProtocol, RejectsUnknownAndDuplicateKeys) {
    std::string error;

    EXPECT_FALSE(CommandProtocol::ParseLine(
        "{\"cmd\":\"start\",\"outputDir\":\"C:/vods\"}", &error).has_value());
    EXPECT_NE(error.find("campo desconhecido: 'outputDir'"), std::string::npos) << error;

    EXPECT_FALSE(CommandProtocol::ParseLine(
        "{\"cmd\":\"start\",\"cmd\":\"stop\"}", &error).has_value());
    EXPECT_NE(error.find("chave duplicada: 'cmd'"), std::string::npos) << error;

    EXPECT_FALSE(CommandProtocol::ParseLine("{\"cmd\":\"stop\",\"extra\":1}", &error).has_value());
    EXPECT_NE(error.find("campo desconhecido: 'extra'"), std::string::npos) << error;
}

TEST(CommandProtocol, RejectsInvalidResolutionAndFpsWithoutSilentFallback) {
    std::string error;

    EXPECT_FALSE(CommandProtocol::ParseLine(
        "{\"cmd\":\"start\",\"output_dir\":\"C:/vods\",\"resolution\":\"480p\"}", &error).has_value());
    EXPECT_NE(error.find("resolução inválida"), std::string::npos) << error;

    EXPECT_FALSE(CommandProtocol::ParseLine(
        "{\"cmd\":\"start\",\"output_dir\":\"C:/vods\",\"fps\":\"45\"}", &error).has_value());
    EXPECT_NE(error.find("fps inválido"), std::string::npos) << error;

    EXPECT_FALSE(CommandProtocol::ParseLine(
        "{\"cmd\":\"start\",\"output_dir\":\"C:/vods\",\"resolution\":\"\"}", &error).has_value());
    EXPECT_NE(error.find("resolução inválida"), std::string::npos) << error;
}

TEST(CommandProtocol, RequiresCmdAndOutputDir) {
    std::string error;

    EXPECT_FALSE(CommandProtocol::ParseLine("{\"resolution\":\"720p\"}", &error).has_value());
    EXPECT_NE(error.find("campo obrigatório ausente: 'cmd'"), std::string::npos) << error;

    EXPECT_FALSE(CommandProtocol::ParseLine("{\"cmd\":\"start\"}", &error).has_value());
    EXPECT_NE(error.find("campo obrigatório ausente: 'output_dir'"), std::string::npos) << error;

    EXPECT_FALSE(CommandProtocol::ParseLine(
        "{\"cmd\":\"start\",\"output_dir\":true}", &error).has_value());
    EXPECT_NE(error.find("campo 'output_dir' deve ser string"), std::string::npos) << error;
}

TEST(CommandProtocol, DecodesUnicodeEscapes) {
    auto c = CommandProtocol::ParseLine(
        "{\"cmd\":\"start\",\"output_dir\":\"C:/vods/\\u00E1udio\"}");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->settings.output_dir, std::string("C:/vods/\xC3\xA1udio"));
}

TEST(CommandProtocol, RejectsInvalidUnicodeEscapes) {
    const char* invalid[] = {
        "{\"cmd\":\"start\",\"output_dir\":\"C:/vods/\\uD800\"}", // surrogate sem par
        "{\"cmd\":\"start\",\"output_dir\":\"C:/vods/\\uDC00\"}", // surrogate baixo
        "{\"cmd\":\"start\",\"output_dir\":\"C:/vods/\\x41\"}"    // escape desconhecido
    };
    for (const char* line : invalid) {
        std::string error;
        EXPECT_FALSE(CommandProtocol::ParseLine(line, &error).has_value()) << line;
    }
}
