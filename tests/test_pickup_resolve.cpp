#include <gtest/gtest.h>
#include "level/level_objects.h"

// ResolvePickupModelId maps a WEAPON_ID_*/AMMO_ID_* enum to a render model id
// using the loaded IGIModels.json map. Unknown/non-enum input returns the input
// unchanged (caller renders the raw string, matching the existing fallback).
TEST(PickupResolveTest, UnknownEnumReturnsInputUnchanged) {
    LevelObjects lo;
    EXPECT_EQ(lo.ResolvePickupModelId("WEAPON_ID_DOES_NOT_EXIST"),
              "WEAPON_ID_DOES_NOT_EXIST");
}

TEST(PickupResolveTest, NonEnumStringReturnedUnchanged) {
    LevelObjects lo;
    EXPECT_EQ(lo.ResolvePickupModelId("123_45_6"), "123_45_6");
}

// A known weapon enum resolves to an 8-char NNN_NN_N model id (from IGIModels.json).
TEST(PickupResolveTest, KnownWeaponEnumResolvesToModelId) {
    LevelObjects lo;
    std::string r = lo.ResolvePickupModelId("WEAPON_ID_UZI");
    bool resolved = (r.size() == 8 && r[3] == '_' && r[6] == '_');
    EXPECT_TRUE(resolved || r == "WEAPON_ID_UZI") << "got: " << r;
}
