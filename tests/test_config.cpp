#include <gtest/gtest.h>
#include "config.h"

// ============================================================
//  Config — full suite
//
//  Config::Init() loads from embedded QVM config data.
//  All checks are against runtime-observable guarantees.
// ============================================================

// Call Init() once before every test so state is fresh.
class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        Config::Init();
    }
};

// ---------- level number ----------

TEST_F(ConfigTest, LevelIsPositive) {
    EXPECT_GT(Config::Get().level, 0);
}

TEST_F(ConfigTest, LevelInValidRange) {
    int lvl = Config::Get().level;
    EXPECT_GE(lvl, 1);
    EXPECT_LE(lvl, 14);
}

// ---------- rendering ----------

TEST_F(ConfigTest, RenderZNearIsPositive) {
    EXPECT_GT(Config::Get().renderZNear, 0.0f);
}

TEST_F(ConfigTest, FontSizeIsPositive) {
    EXPECT_GT(Config::Get().fontSize, 0.0f);
}

TEST_F(ConfigTest, FontColorChannelsInRange) {
    auto& d = Config::Get();
    EXPECT_GE(d.fontColorR, 0);   EXPECT_LE(d.fontColorR, 255);
    EXPECT_GE(d.fontColorG, 0);   EXPECT_LE(d.fontColorG, 255);
    EXPECT_GE(d.fontColorB, 0);   EXPECT_LE(d.fontColorB, 255);
}

TEST_F(ConfigTest, SystemFontSizeIsValidGlutSize) {
    // GLUT system font supports 10, 12, or 18 pt
    int sz = Config::Get().systemFontSize;
    EXPECT_TRUE(sz == 10 || sz == 12 || sz == 18)
        << "unexpected systemFontSize: " << sz;
}

// ---------- singleton ----------

TEST_F(ConfigTest, GetReturnsSameInstance) {
    auto& a = Config::Get();
    auto& b = Config::Get();
    EXPECT_EQ(&a, &b);
}

TEST_F(ConfigTest, MultipleInitCallsAreSafe) {
    // Init can be called repeatedly without crashing
    Config::Init();
    Config::Init();
    EXPECT_GT(Config::Get().level, 0);
}

// ---------- keybindings structure ----------

TEST_F(ConfigTest, KeybindingsHaveNonZeroVkCodes) {
    // At least one keybinding should have a non-zero VK code,
    // confirming keybinding data was loaded.
    auto& d = Config::Get();
    bool any_nonzero =
        d.keySave.vkCode   != 0 ||
        d.keyQuit.vkCode   != 0 ||
        d.keyUndo.vkCode   != 0 ||
        d.keyRedo.vkCode   != 0;
    EXPECT_TRUE(any_nonzero) << "all keybinding vkCodes are 0 — config may not have loaded";
}

// ---------- interpolation ----------

TEST_F(ConfigTest, InterpolationIsNonNegative) {
    EXPECT_GE(Config::Get().interpolation, 0);
}
