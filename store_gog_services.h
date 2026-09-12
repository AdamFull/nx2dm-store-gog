#pragma once

#include "store_gog/store_gog_platform.h"

#include "store/store_service.h"

#include <galaxy/GalaxyApi.h>

namespace nxm::store_gog {

/// Four of the five neutral services (store_service.h), backed by the real
/// GOG Galaxy SDK - store.iap is deliberately never registered (see
/// GogModule / the module's own comment), the same "simply doesn't provide
/// it" shape a backend missing cloud saves or presence already has
/// elsewhere in this module family, just for a different service: the
/// vendored Galaxy SDK (1.152.10) has no purchase/checkout API at all,
/// confirmed absent from every header - only entitlement checks
/// (IApps::IsDlcOwned()/IsDlcInstalled()), which store.core already covers.
///
/// Most of Galaxy's calls are synchronous local-cache reads (IStorage,
/// IFriends' own persona/friend accessors, IStats' Get/Set once a session is
/// signed in) - closer to Steam's shape than EOS's - but a few genuinely
/// need a listener: DLC ownership (IApps::IsDlcOwned), the
/// stats/achievements cache load (IStats::RequestUserStatsAndAchievements),
/// and the friends list (IFriends::RequestFriendList). Every class below
/// checks GogPlatform::logged_on() first and degrades to a safe default
/// when it's false - true whenever no Galaxy Client is installed/running,
/// exactly the guard path store_steam's tests already established for "no
/// live client in this environment".
///
/// Galaxy has no per-call success/failure return value - every API call
/// resets a thread-local error read back via galaxy::api::GetError() -
/// so every method here checks that after each call, the same role a
/// Steamworks bool return or an EOS_EResult already plays in the other two
/// backends.

class GogCore final : public store::StoreCore {
public:
  explicit GogCore(GogPlatform &platform) noexcept : m_platform(platform) {}

  [[nodiscard]] bool is_owned(nx::string_view dlc_id = {}) const override;
  [[nodiscard]] nx::vector<nx::string> owned_dlc_ids() const override {
    return m_owned_dlc_ids;
  }
  [[nodiscard]] nx::string_view store_name() const noexcept override {
    return "gog";
  }

  /// GOG's SDK has no DLC catalogue enumeration (confirmed absent - there is
  /// no GetDlcCount()/GetDlcByIndex() or similar) and no full-game ownership
  /// call either - a game must know its own DLC ProductIDs up front (@p
  /// dlc_id is parsed as a decimal galaxy::api::ProductID) and probe each
  /// one explicitly. Fires IApps::IsDlcOwned(), refreshing owned_dlc_ids().
  void check_dlc_owned(nx::string_view dlc_id);

  /// Galaxy has no bulk ownership query - an empty @p dlc_id is a no-op
  /// here, unlike every other backend's store::StoreCore::refresh_ownership()
  /// override, which treats an empty id as "refresh everything".
  void refresh_ownership(const nx::string_view dlc_id = {}) override {
    if (!dlc_id.empty())
      check_dlc_owned(dlc_id);
  }

private:
  class DlcOwnedListener;

  void on_dlc_check_result(const nx::string &dlc_id, bool owned);

  GogPlatform &m_platform;
  nx::vector<nx::string> m_owned_dlc_ids;
};

class GogAchievements final : public store::StoreAchievements {
public:
  explicit GogAchievements(GogPlatform &platform) noexcept
      : m_platform(platform) {}

  bool unlock(nx::string_view id) override;
  [[nodiscard]] bool is_unlocked(nx::string_view id) const override;
  [[nodiscard]] nx::vector<nx::string> achievement_ids() const override;
  bool set_stat(nx::string_view id, f64 value) override;
  [[nodiscard]] f64 stat(nx::string_view id) const override;

  /// Fires IStats::RequestUserStatsAndAchievements(), populating Galaxy's
  /// own local cache that every method above reads/writes through - without
  /// this having completed at least once, Get*/GetAchievement* fall back to
  /// their own defaults rather than a real value, the same
  /// eventually-consistent shape store_egs's refresh_* methods already
  /// document.
  void refresh_stats_and_achievements();

  /// Galaxy already queries every stat/achievement in one round trip -
  /// @p stat_ids is ignored, same as store::StoreAchievements::refresh()
  /// documents for any bulk-capable backend.
  void refresh(const nx::vector<nx::string> & = {}) override {
    refresh_stats_and_achievements();
  }

private:
  class StatsRetrieveListener;

  GogPlatform &m_platform;
};

/// Small key/value cloud saves via IStorage, the simpler of GOG's two
/// storage APIs (the other, ICloudStorage, is container-based and
/// async-only, but is the only one exposing quota/usage bytes - not used
/// here, see bytes_used()/bytes_total() below) and the closer match to this
/// interface's shape: synchronous, no listeners, files synced automatically
/// by the Galaxy Client in the background.
class GogCloudSaves final : public store::StoreCloudSaves {
public:
  explicit GogCloudSaves(GogPlatform &platform) noexcept
      : m_platform(platform) {}

  bool write(nx::string_view key, nx::string_view value) override;
  [[nodiscard]] nx::string read(nx::string_view key) const override;
  [[nodiscard]] bool exists(nx::string_view key) const override;
  bool remove(nx::string_view key) override;
  [[nodiscard]] nx::vector<nx::string> keys() const override;
  /// IStorage has no quota/usage query at all - GOG only exposes that
  /// through the separate, container-based ICloudStorage interface
  /// (ICloudStorageGetFileListListener::OnGetFileListSuccess's quota/
  /// quotaUsed parameters), a different async API this class does not
  /// otherwise use - so these two always report 0, the same honest gap
  /// store_egs's PlayerDataStorage already has.
  [[nodiscard]] u64 bytes_used() const override { return 0; }
  [[nodiscard]] u64 bytes_total() const override { return 0; }

private:
  GogPlatform &m_platform;
};

class GogPresence final : public store::StorePresence {
public:
  explicit GogPresence(GogPlatform &platform) noexcept : m_platform(platform) {}

  bool set_status(nx::string_view text) override;
  [[nodiscard]] nx::string_view own_name() const override;
  [[nodiscard]] usize friend_count() const override;
  [[nodiscard]] nx::vector<nx::string> friend_names() const override;

  /// Fires IFriends::RequestFriendList() - needed before friend_count()/
  /// friend_names() report anything real, the same eventually-consistent
  /// shape refresh_stats_and_achievements() above documents.
  void refresh_friends();

  void refresh() override { refresh_friends(); }

private:
  class FriendListListener;

  GogPlatform &m_platform;
};

}
