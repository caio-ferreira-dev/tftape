#include <gtest/gtest.h>

#include "audio/AudioRouting.h"

using namespace tft;

TEST(AudioRoutingTests, DefaultOptionsEnableGameOnlyWithIsolation) {
    AudioOptions audio; // game_audio = true por padrão
    const AudioRoutingPlan plan = PlanAudioRouting(audio, 4321);

    EXPECT_TRUE(plan.game);
    EXPECT_FALSE(plan.system);
    EXPECT_FALSE(plan.mic);
    EXPECT_EQ(plan.game_isolation_pid, 4321u);
    EXPECT_EQ(plan.system_exclude_pid, 0u);
}

TEST(AudioRoutingTests, SystemAudioExcludesGameProcessTree) {
    AudioOptions audio;
    audio.game_audio = true;
    audio.system_audio = true;

    const AudioRoutingPlan plan = PlanAudioRouting(audio, 4321);

    EXPECT_TRUE(plan.game);
    EXPECT_TRUE(plan.system);
    EXPECT_EQ(plan.game_isolation_pid, 4321u);
    EXPECT_EQ(plan.system_exclude_pid, 4321u);
}

TEST(AudioRoutingTests, SystemAudioOnlyStillExcludesGameProcessTree) {
    AudioOptions audio;
    audio.game_audio = false;
    audio.system_audio = true;

    const AudioRoutingPlan plan = PlanAudioRouting(audio, 99);

    EXPECT_FALSE(plan.game);
    EXPECT_TRUE(plan.system);
    EXPECT_EQ(plan.game_isolation_pid, 0u);
    EXPECT_EQ(plan.system_exclude_pid, 99u);
}

TEST(AudioRoutingTests, NoProcessIdDisablesIsolation) {
    AudioOptions audio;
    audio.game_audio = true;
    audio.system_audio = true;

    const AudioRoutingPlan plan = PlanAudioRouting(audio, 0);

    EXPECT_TRUE(plan.game);
    EXPECT_TRUE(plan.system);
    EXPECT_EQ(plan.game_isolation_pid, 0u);
    EXPECT_EQ(plan.system_exclude_pid, 0u);
}

TEST(AudioRoutingTests, AllOptionsDisabled) {
    AudioOptions audio;
    audio.game_audio = false;
    audio.system_audio = false;
    audio.microphone = false;

    const AudioRoutingPlan plan = PlanAudioRouting(audio, 123);

    EXPECT_FALSE(plan.game);
    EXPECT_FALSE(plan.system);
    EXPECT_FALSE(plan.mic);
    EXPECT_EQ(plan.game_isolation_pid, 0u);
    EXPECT_EQ(plan.system_exclude_pid, 0u);
}

TEST(AudioRoutingTests, MicrophoneOnly) {
    AudioOptions audio;
    audio.game_audio = false;
    audio.system_audio = false;
    audio.microphone = true;

    const AudioRoutingPlan plan = PlanAudioRouting(audio, 123);

    EXPECT_FALSE(plan.game);
    EXPECT_FALSE(plan.system);
    EXPECT_TRUE(plan.mic);
    EXPECT_EQ(plan.game_isolation_pid, 0u);
    EXPECT_EQ(plan.system_exclude_pid, 0u);
}

TEST(AudioDeduplicationTests, KeepsBothWhenBothAreProcessIsolated) {
    AudioOptions audio;
    audio.game_audio = true;
    audio.system_audio = true;
    const AudioRoutingPlan plan = PlanAudioRouting(audio, 10);

    const AudioSourceSelection sel = DeduplicateAudioSources(plan, true, true, true, true);

    EXPECT_TRUE(sel.keep_game);
    EXPECT_TRUE(sel.keep_system);
}

TEST(AudioDeduplicationTests, DropsGameWhenSystemFallsBackToDeviceLoopback) {
    AudioOptions audio;
    audio.game_audio = true;
    audio.system_audio = true;
    const AudioRoutingPlan plan = PlanAudioRouting(audio, 10);

    const AudioSourceSelection sel = DeduplicateAudioSources(plan, true, true, true, false);

    EXPECT_FALSE(sel.keep_game);
    EXPECT_TRUE(sel.keep_system);
}

TEST(AudioDeduplicationTests, DropsSystemWhenGameFallsBackToDeviceLoopback) {
    AudioOptions audio;
    audio.game_audio = true;
    audio.system_audio = true;
    const AudioRoutingPlan plan = PlanAudioRouting(audio, 10);

    const AudioSourceSelection sel = DeduplicateAudioSources(plan, true, false, true, true);

    EXPECT_TRUE(sel.keep_game);
    EXPECT_FALSE(sel.keep_system);
}

TEST(AudioDeduplicationTests, KeepsOnlySystemWhenNeitherIsIsolated) {
    AudioOptions audio;
    audio.game_audio = true;
    audio.system_audio = true;
    const AudioRoutingPlan plan = PlanAudioRouting(audio, 10);

    const AudioSourceSelection sel = DeduplicateAudioSources(plan, true, false, true, false);

    EXPECT_FALSE(sel.keep_game);
    EXPECT_TRUE(sel.keep_system);
}

TEST(AudioDeduplicationTests, KeepsSingleRequestedSource) {
    AudioOptions game_only;
    const AudioRoutingPlan game_plan = PlanAudioRouting(game_only, 10);
    const AudioSourceSelection game_sel = DeduplicateAudioSources(game_plan, true, true, false, false);
    EXPECT_TRUE(game_sel.keep_game);
    EXPECT_FALSE(game_sel.keep_system);

    AudioOptions system_only;
    system_only.game_audio = false;
    system_only.system_audio = true;
    const AudioRoutingPlan system_plan = PlanAudioRouting(system_only, 10);
    const AudioSourceSelection system_sel = DeduplicateAudioSources(system_plan, false, false, true, false);
    EXPECT_FALSE(system_sel.keep_game);
    EXPECT_TRUE(system_sel.keep_system);
}

TEST(AudioDeduplicationTests, DoesNotKeepInactiveRequestedSource) {
    AudioOptions audio;
    audio.game_audio = true;
    audio.system_audio = true;
    const AudioRoutingPlan plan = PlanAudioRouting(audio, 10);

    // Sistema falhou ao iniciar; jogo permanece isolado.
    const AudioSourceSelection sel = DeduplicateAudioSources(plan, true, true, false, false);

    EXPECT_TRUE(sel.keep_game);
    EXPECT_FALSE(sel.keep_system);
}

TEST(AudioStartCheckTests, GameAudioFailureBlocksRecording) {
    AudioOptions audio;
    const AudioRoutingPlan plan = PlanAudioRouting(audio, 10);

    const AudioStartCheck check = EvaluateAudioStart(plan, /*game=*/false, /*system=*/false, /*mic=*/false);

    EXPECT_FALSE(check.can_record);
    EXPECT_FALSE(check.error.empty());
    EXPECT_TRUE(check.warnings.empty());
}

TEST(AudioStartCheckTests, OptionalSourcesFailAsWarnings) {
    AudioOptions audio;
    audio.system_audio = true;
    audio.microphone = true;
    const AudioRoutingPlan plan = PlanAudioRouting(audio, 10);

    const AudioStartCheck check = EvaluateAudioStart(plan, /*game=*/true, /*system=*/false, /*mic=*/false);

    EXPECT_TRUE(check.can_record);
    EXPECT_TRUE(check.error.empty());
    ASSERT_EQ(check.warnings.size(), 2u);
}

TEST(AudioStartCheckTests, AllSourcesHealthyHasNoWarnings) {
    AudioOptions audio;
    audio.system_audio = true;
    audio.microphone = true;
    const AudioRoutingPlan plan = PlanAudioRouting(audio, 10);

    const AudioStartCheck check = EvaluateAudioStart(plan, true, true, true);

    EXPECT_TRUE(check.can_record);
    EXPECT_TRUE(check.warnings.empty());
}

TEST(AudioStartCheckTests, DisabledSourcesDoNotWarn) {
    AudioOptions audio;
    audio.game_audio = false;
    const AudioRoutingPlan plan = PlanAudioRouting(audio, 10);

    const AudioStartCheck check = EvaluateAudioStart(plan, false, false, false);

    EXPECT_TRUE(check.can_record);
    EXPECT_TRUE(check.warnings.empty());
}
