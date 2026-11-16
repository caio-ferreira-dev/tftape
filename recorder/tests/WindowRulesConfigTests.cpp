#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "capture/WindowRules.h"

namespace fs = std::filesystem;
using tft::WindowRuleConfig;

TEST(WindowRulesConfig, ParsesJsonArrays) {
    WindowRuleConfig cfg;
    cfg.title_contains.clear();
    cfg.process_candidates.clear();
    cfg.process_excludes.clear();
    std::string json = R"({
      "title_contains": ["Teamfight Tactics", "TFT"],
      "process_candidates": ["TftClient.exe"],
      "process_excludes": ["LeagueClient.exe"]
    })";
    std::string err;
    ASSERT_TRUE(WindowRuleConfig::ParseJson(json, cfg, &err)) << err;
    ASSERT_EQ(cfg.title_contains.size(), 2u);
    EXPECT_EQ(cfg.title_contains[0], "Teamfight Tactics");
    ASSERT_EQ(cfg.process_candidates.size(), 1u);
    EXPECT_EQ(cfg.process_candidates[0], "TftClient.exe");
    ASSERT_EQ(cfg.process_excludes.size(), 1u);
    EXPECT_EQ(cfg.process_excludes[0], "LeagueClient.exe");
}

TEST(WindowRulesConfig, KeepsDefaultsForMissingFields) {
    WindowRuleConfig cfg; // defaults
    auto defaults = cfg.process_candidates.size();
    std::string json = R"({"title_contains": ["tft"]})";
    ASSERT_TRUE(WindowRuleConfig::ParseJson(json, cfg, nullptr));
    ASSERT_EQ(cfg.title_contains.size(), 1u);
    EXPECT_EQ(cfg.process_candidates.size(), defaults);
}

TEST(WindowRulesConfig, LoadsFromFile) {
    auto p = fs::temp_directory_path() / "tft_rules_test.json";
    {
        std::ofstream out(p.string(), std::ios::binary);
        out << R"({"process_candidates":["MyTft.exe"]})";
    }
    WindowRuleConfig cfg;
    std::string err;
    ASSERT_TRUE(WindowRuleConfig::LoadFromFile(p.string(), cfg, &err)) << err;
    ASSERT_EQ(cfg.process_candidates.size(), 1u);
    EXPECT_EQ(cfg.process_candidates[0], "MyTft.exe");
    fs::remove(p);
}

TEST(WindowRulesConfig, FailsOnMissingFile) {
    WindowRuleConfig cfg;
    std::string err;
    EXPECT_FALSE(WindowRuleConfig::LoadFromFile("Z:/no/such/tft_rules.json", cfg, &err));
    EXPECT_FALSE(err.empty());
}
