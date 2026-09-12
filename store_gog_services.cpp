#include "store_gog/store_gog_services.h"

#include "core/foundation/diagnostics/log.h"

#include <utility>

namespace nxm::store_gog {
namespace {

const nx::log::Category log_store_gog = nx::log::category("store_gog");

[[nodiscard]] bool parse_product_id(const nx::string_view text,
                                     galaxy::api::ProductID &out) {
  if (text.empty())
    return false;
  u64 value = 0;
  for (const char c : text) {
    if (c < '0' || c > '9')
      return false;
    value = value * 10 + static_cast<u64>(c - '0');
  }
  out = value;
  return true;
}

} // namespace

// -- GogCore --------------------------------------------------------------

class GogCore::DlcOwnedListener final
    : public galaxy::api::IIsDlcOwnedListener {
public:
  DlcOwnedListener(GogCore &owner, nx::string dlc_id)
      : m_owner(owner), m_dlc_id(std::move(dlc_id)) {}

  void OnDlcCheckSuccess(galaxy::api::ProductID, const bool is_owned) override {
    m_owner.on_dlc_check_result(m_dlc_id, is_owned);
    delete this;
  }
  void OnDlcCheckFailure(galaxy::api::ProductID,
                          const FailureReason failure_reason) override {
    nx::logi(log_store_gog, "IsDlcOwned('{}') failed: {}", m_dlc_id.view(),
              static_cast<int>(failure_reason));
    delete this;
  }

private:
  GogCore &m_owner;
  nx::string m_dlc_id;
};

bool GogCore::is_owned(const nx::string_view dlc_id) const {
  if (!m_platform.logged_on())
    return false;
  if (dlc_id.empty())
    // Galaxy has no full-game ownership call, but SignInGalaxy() itself
    // already fails with FAILURE_REASON_NO_LICENSE when the signed-in user
    // doesn't own this game - reaching logged_on() at all is proof enough.
    return true;
  for (const nx::string &owned : m_owned_dlc_ids)
    if (owned.view() == dlc_id)
      return true;
  return false;
}

void GogCore::check_dlc_owned(const nx::string_view dlc_id) {
  galaxy::api::ProductID product_id = 0;
  if (!m_platform.logged_on() || !parse_product_id(dlc_id, product_id))
    return;
  galaxy::api::Apps()->IsDlcOwned(
      product_id, new DlcOwnedListener(*this, nx::string(dlc_id)));
}

void GogCore::on_dlc_check_result(const nx::string &dlc_id, const bool owned) {
  usize existing = m_owned_dlc_ids.size();
  for (usize i = 0; i < m_owned_dlc_ids.size(); ++i) {
    if (m_owned_dlc_ids[i].view() == dlc_id.view()) {
      existing = i;
      break;
    }
  }
  if (owned) {
    if (existing == m_owned_dlc_ids.size())
      m_owned_dlc_ids.push_back(dlc_id);
  } else if (existing != m_owned_dlc_ids.size()) {
    m_owned_dlc_ids.erase(m_owned_dlc_ids.begin() +
                           static_cast<isize>(existing));
  }
}

// -- GogAchievements --------------------------------------------------------

class GogAchievements::StatsRetrieveListener final
    : public galaxy::api::IUserStatsAndAchievementsRetrieveListener {
public:
  void OnUserStatsAndAchievementsRetrieveSuccess(galaxy::api::GalaxyID) override {
    delete this;
  }
  void OnUserStatsAndAchievementsRetrieveFailure(
      galaxy::api::GalaxyID, const FailureReason failure_reason) override {
    nx::logi(log_store_gog, "RequestUserStatsAndAchievements failed: {}",
              static_cast<int>(failure_reason));
    delete this;
  }
};

bool GogAchievements::unlock(const nx::string_view id) {
  if (!m_platform.logged_on())
    return false;
  galaxy::api::IStats *const stats = galaxy::api::Stats();
  const nx::string name(id);
  stats->SetAchievement(name.c_str());
  if (galaxy::api::GetError() != nullptr)
    return false;
  // StoreStatsAndAchievements() itself is async - this reports whether
  // kicking off the persist succeeded, not that it has landed on Galaxy's
  // backend yet, the same honest limitation store_egs's IAP checkout
  // already documents for its own fire-and-forget call.
  stats->StoreStatsAndAchievements();
  return galaxy::api::GetError() == nullptr;
}

bool GogAchievements::is_unlocked(const nx::string_view id) const {
  if (!m_platform.logged_on())
    return false;
  const nx::string name(id);
  bool unlocked = false;
  uint32_t unlock_time = 0;
  galaxy::api::Stats()->GetAchievement(name.c_str(), unlocked, unlock_time);
  return galaxy::api::GetError() == nullptr && unlocked;
}

nx::vector<nx::string> GogAchievements::achievement_ids() const {
  nx::vector<nx::string> out;
  if (!m_platform.logged_on())
    return out;
  galaxy::api::IStats *const stats = galaxy::api::Stats();
  const uint32_t count = stats->GetAchievementsNumber();
  if (galaxy::api::GetError() != nullptr)
    return out;
  out.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    const char *const name = stats->GetAchievementName(i);
    if (galaxy::api::GetError() == nullptr && name != nullptr)
      out.emplace_back(name);
  }
  return out;
}

// SetStatInt/GetStatInt and SetStatFloat/GetStatFloat are separate methods,
// tied to how the stat was declared on GOG's backend - a caller here has no
// way to know which, so int is tried first and float second, the same
// fallback order store_steam's own SetStat/GetStat overload pair already
// uses for the identical ambiguity.
bool GogAchievements::set_stat(const nx::string_view id, const f64 value) {
  if (!m_platform.logged_on())
    return false;
  galaxy::api::IStats *const stats = galaxy::api::Stats();
  const nx::string name(id);
  stats->SetStatInt(name.c_str(), static_cast<int32_t>(value));
  if (galaxy::api::GetError() != nullptr) {
    stats->SetStatFloat(name.c_str(), static_cast<float>(value));
    if (galaxy::api::GetError() != nullptr)
      return false;
  }
  stats->StoreStatsAndAchievements();
  return galaxy::api::GetError() == nullptr;
}

f64 GogAchievements::stat(const nx::string_view id) const {
  if (!m_platform.logged_on())
    return 0.0;
  galaxy::api::IStats *const stats = galaxy::api::Stats();
  const nx::string name(id);
  const int32_t as_int = stats->GetStatInt(name.c_str());
  if (galaxy::api::GetError() == nullptr)
    return static_cast<f64>(as_int);
  const float as_float = stats->GetStatFloat(name.c_str());
  return galaxy::api::GetError() == nullptr ? static_cast<f64>(as_float) : 0.0;
}

void GogAchievements::refresh_stats_and_achievements() {
  if (!m_platform.logged_on())
    return;
  galaxy::api::Stats()->RequestUserStatsAndAchievements(
      galaxy::api::GalaxyID(), new StatsRetrieveListener());
}

// -- GogCloudSaves ----------------------------------------------------------

bool GogCloudSaves::write(const nx::string_view key,
                           const nx::string_view value) {
  if (!m_platform.logged_on())
    return false;
  const nx::string name(key);
  galaxy::api::Storage()->FileWrite(name.c_str(), value.data(),
                                     static_cast<uint32_t>(value.size()));
  return galaxy::api::GetError() == nullptr;
}

nx::string GogCloudSaves::read(const nx::string_view key) const {
  if (!m_platform.logged_on())
    return {};
  galaxy::api::IStorage *const storage = galaxy::api::Storage();
  const nx::string name(key);
  const uint32_t size = storage->GetFileSize(name.c_str());
  if (galaxy::api::GetError() != nullptr || size == 0)
    return {};
  nx::string out;
  out.resize(static_cast<usize>(size));
  const uint32_t read = storage->FileRead(name.c_str(), out.data(), size);
  if (galaxy::api::GetError() != nullptr || read == 0)
    return {};
  out.resize(static_cast<usize>(read));
  return out;
}

bool GogCloudSaves::exists(const nx::string_view key) const {
  if (!m_platform.logged_on())
    return false;
  const nx::string name(key);
  const bool exists = galaxy::api::Storage()->FileExists(name.c_str());
  return galaxy::api::GetError() == nullptr && exists;
}

bool GogCloudSaves::remove(const nx::string_view key) {
  if (!m_platform.logged_on())
    return false;
  const nx::string name(key);
  galaxy::api::Storage()->FileDelete(name.c_str());
  return galaxy::api::GetError() == nullptr;
}

nx::vector<nx::string> GogCloudSaves::keys() const {
  nx::vector<nx::string> out;
  if (!m_platform.logged_on())
    return out;
  galaxy::api::IStorage *const storage = galaxy::api::Storage();
  const uint32_t count = storage->GetFileCount();
  if (galaxy::api::GetError() != nullptr)
    return out;
  out.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    const char *const name = storage->GetFileNameByIndex(i);
    if (galaxy::api::GetError() == nullptr && name != nullptr)
      out.emplace_back(name);
  }
  return out;
}

// -- GogPresence --------------------------------------------------------

class GogPresence::FriendListListener final
    : public galaxy::api::IFriendListListener {
public:
  void OnFriendListRetrieveSuccess() override { delete this; }
  void OnFriendListRetrieveFailure(const FailureReason failure_reason) override {
    nx::logi(log_store_gog, "RequestFriendList failed: {}",
              static_cast<int>(failure_reason));
    delete this;
  }
};

bool GogPresence::set_status(const nx::string_view text) {
  if (!m_platform.logged_on())
    return false;
  const nx::string value(text);
  galaxy::api::Friends()->SetRichPresence("status", value.c_str());
  return galaxy::api::GetError() == nullptr;
}

nx::string_view GogPresence::own_name() const {
  if (!m_platform.logged_on())
    return {};
  const char *const name = galaxy::api::Friends()->GetPersonaName();
  return galaxy::api::GetError() == nullptr && name != nullptr
             ? nx::string_view(name)
             : nx::string_view{};
}

usize GogPresence::friend_count() const {
  if (!m_platform.logged_on())
    return 0;
  const uint32_t count = galaxy::api::Friends()->GetFriendCount();
  return galaxy::api::GetError() == nullptr ? static_cast<usize>(count) : 0;
}

nx::vector<nx::string> GogPresence::friend_names() const {
  nx::vector<nx::string> out;
  if (!m_platform.logged_on())
    return out;
  galaxy::api::IFriends *const friends = galaxy::api::Friends();
  const uint32_t count = friends->GetFriendCount();
  if (galaxy::api::GetError() != nullptr)
    return out;
  out.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    const galaxy::api::GalaxyID id = friends->GetFriendByIndex(i);
    if (galaxy::api::GetError() != nullptr)
      continue;
    const char *const name = friends->GetFriendPersonaName(id);
    if (galaxy::api::GetError() == nullptr && name != nullptr)
      out.emplace_back(name);
  }
  return out;
}

void GogPresence::refresh_friends() {
  if (!m_platform.logged_on())
    return;
  galaxy::api::Friends()->RequestFriendList(new FriendListListener());
}

}
