#include <gtest/gtest.h>

#include "capture/WindowRules.h"

using tft::MatchStatus;
using tft::WindowInfo;
using tft::WindowRules;

TEST(WindowRulesMatch, MatchesTftClientByProcess) {
    WindowRules rules;
    std::vector<WindowInfo> wins{{"Teamfight Tactics", "TftClient.exe", "", true, 1920, 1080, 1}};
    auto r = rules.SelectTftWindow(wins);
    EXPECT_EQ(r.status, MatchStatus::Matched);
    ASSERT_TRUE(r.window.has_value());
    EXPECT_EQ(r.window->handle_id, 1u);
    EXPECT_NE(r.matched_rule.find("process_candidates"), std::string::npos);
    ASSERT_EQ(r.evaluations.size(), 1u);
    EXPECT_TRUE(r.evaluations[0].candidate);
    EXPECT_FALSE(r.evaluations[0].excluded);
}

TEST(WindowRulesMatch, ExcludesLeagueOfLegendsWindow) {
    WindowRules rules;
    std::vector<WindowInfo> wins{{"League of Legends", "League of Legends.exe", "", true, 1920, 1080, 2}};
    auto r = rules.SelectTftWindow(wins);
    EXPECT_EQ(r.status, MatchStatus::NoCandidate);
    ASSERT_EQ(r.evaluations.size(), 1u);
    EXPECT_TRUE(r.evaluations[0].excluded);
    EXPECT_NE(r.evaluations[0].reason.find("process_excludes"), std::string::npos);
}

TEST(WindowRulesMatch, ReturnsNoCandidateWhenMissing) {
    WindowRules rules;
    std::vector<WindowInfo> wins{{"Browser", "chrome.exe", "", true, 1280, 720, 3}};
    auto r = rules.SelectTftWindow(wins);
    EXPECT_EQ(r.status, MatchStatus::NoCandidate);
}

TEST(WindowRulesMatch, ReturnsAmbiguousOnMultipleCandidates) {
    WindowRules rules;
    std::vector<WindowInfo> wins{
        {"Teamfight Tactics", "TftClient.exe", "", true, 1920, 1080, 1},
        {"Teamfight Tactics", "TftClient-Win64-Shipping.exe", "", true, 1280, 720, 2},
    };
    auto r = rules.SelectTftWindow(wins);
    EXPECT_EQ(r.status, MatchStatus::Ambiguous);
}

TEST(WindowRulesMatch, RejectsNotCapturableDimensions) {
    WindowRules rules;
    std::vector<WindowInfo> wins{{"Teamfight Tactics", "TftClient.exe", "", true, 0, 0, 1}};
    auto r = rules.SelectTftWindow(wins);
    EXPECT_EQ(r.status, MatchStatus::NotCapturable);
}

TEST(WindowRulesMatch, MatchesByTitleWhenNotExcluded) {
    WindowRules rules;
    std::vector<WindowInfo> wins{{"Teamfight Tactics", "SomeGameHost.exe", "", true, 1600, 900, 7}};
    auto r = rules.SelectTftWindow(wins);
    EXPECT_EQ(r.status, MatchStatus::Matched);
    EXPECT_NE(r.matched_rule.find("title_contains"), std::string::npos);
}
