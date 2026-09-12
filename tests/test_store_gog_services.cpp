#include "framework/nxtest.h"

#include "store_gog/store_gog_platform.h"
#include "store_gog/store_gog_services.h"

// Like store_egs's EgsPlatform::initialize(), GogPlatform::initialize()
// drives a real SignInGalaxy() call against a locally installed Galaxy
// Client - not something to fire in a unit test. Every service method here
// checks GogPlatform::logged_on() before touching any Galaxy interface at
// all, so a never-initialized GogPlatform (always false) proves the same
// guard-path bar as store_steam's and store_egs's own tests without ever
// making a real Galaxy call.

using namespace nxm::store_gog;

TEST_CASE("store_gog services: a never-initialized platform reports "
          "correctly") {
  const GogPlatform platform;
  CHECK_FALSE(platform.ready());
  CHECK_FALSE(platform.logged_on());
}

TEST_CASE("store_gog services: GogCore refuses safely with no platform") {
  GogPlatform platform;
  GogCore core(platform);
  CHECK_FALSE(core.is_owned());
  CHECK_FALSE(core.is_owned("12345"));
  CHECK(core.owned_dlc_ids().empty());
  CHECK(core.store_name() == "gog");
  core.check_dlc_owned("12345");
  CHECK(core.owned_dlc_ids().empty());
  // A non-numeric id can't be parsed into a galaxy::api::ProductID either -
  // must refuse just as safely, not crash on the parse.
  core.check_dlc_owned("not-a-number");
  CHECK(core.owned_dlc_ids().empty());
}

TEST_CASE(
    "store_gog services: GogAchievements refuses safely with no platform") {
  GogPlatform platform;
  GogAchievements achievements(platform);
  CHECK_FALSE(achievements.unlock("first_win"));
  CHECK_FALSE(achievements.is_unlocked("first_win"));
  CHECK(achievements.achievement_ids().empty());
  CHECK_FALSE(achievements.set_stat("enemies_killed", 5.0));
  CHECK(achievements.stat("enemies_killed") == 0.0);
  achievements.refresh_stats_and_achievements();
  CHECK(achievements.achievement_ids().empty());
}

TEST_CASE(
    "store_gog services: GogCloudSaves refuses safely with no platform") {
  GogPlatform platform;
  GogCloudSaves saves(platform);
  CHECK_FALSE(saves.write("slot1", "data"));
  CHECK(saves.read("slot1").empty());
  CHECK_FALSE(saves.exists("slot1"));
  CHECK_FALSE(saves.remove("slot1"));
  CHECK(saves.keys().empty());
  CHECK(saves.bytes_used() == 0u);
  CHECK(saves.bytes_total() == 0u);
}

TEST_CASE(
    "store_gog services: GogPresence refuses safely with no platform") {
  GogPlatform platform;
  GogPresence presence(platform);
  CHECK_FALSE(presence.set_status("in menu"));
  CHECK(presence.own_name().empty());
  CHECK(presence.friend_count() == 0u);
  CHECK(presence.friend_names().empty());
  presence.refresh_friends();
  CHECK(presence.friend_names().empty());
}
