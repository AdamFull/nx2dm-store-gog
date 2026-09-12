#pragma once

#include "core/foundation/strings/utf8_string.h"

#include <galaxy/GalaxyApi.h>

#include <memory>

namespace nxm::store_gog {

struct PlatformConfig {
  nx::string client_id;
  nx::string client_secret;
};

/// Owns galaxy::api::Init()/Shutdown()/ProcessData() and the one sign-in flow
/// that fits a real desktop session: IUser::SignInGalaxy(), which
/// authenticates against a locally installed, running GOG Galaxy Client -
/// the same "needs a real client" shape store_steam's SteamAPI_Init()
/// already has. Unlike EOS there is no device-id/headless login option at
/// all here (SignInCredentials() exists but is explicitly documented
/// "testing only", and every other SignIn* overload targets a specific
/// non-desktop platform) - failure to sign in (no Galaxy Client running, or
/// the user has no license) is expected, not an error, and every service in
/// store_gog_services.h degrades to a safe default exactly like
/// store_steam without a Steam client, or store_egs without a cached Epic
/// login.
///
/// Init()/Shutdown() are synchronous, but almost everything past that point
/// - sign-in, ownership checks, stats/achievements, friends - is async via
/// listeners, resolved only while galaxy::api::ProcessData() is pumped
/// (this module's PUMP_SYSTEM calls tick() every frame, the same role
/// store_steam's SteamAPI_RunCallbacks()/store_egs's EOS_Platform_Tick()
/// already play). Galaxy has no per-call return-value error signaling
/// either - every API call resets a thread-local error, checked via
/// galaxy::api::GetError() after each one; store_gog_services.h does this
/// after every call that can fail.
class GogPlatform {
public:
  // Both declared here and defined out-of-line (after AuthListener is a
  // complete type) - std::unique_ptr<AuthListener>'s destructor must be
  // instantiated somewhere that can see AuthListener's definition, and an
  // implicitly-defined default constructor/destructor would otherwise be
  // instantiated inline at every call site (store_gog_module.cpp, this
  // class's own tests) where AuthListener is still incomplete.
  GogPlatform() noexcept;
  ~GogPlatform();

  bool initialize(const PlatformConfig &config);
  void tick();
  void shutdown();

  [[nodiscard]] bool ready() const noexcept { return m_initialized; }
  /// Signed in to the local Galaxy Client *and* logged on to Galaxy's
  /// backend services - the precondition every IApps/IStats/IStorage/
  /// IFriends call in store_gog_services.h needs (IUser::IsLoggedOn(), not
  /// just SignedIn()).
  [[nodiscard]] bool logged_on() const noexcept;

private:
  class AuthListener;

  void on_auth_success();
  void on_auth_failure(int failure_reason);
  void on_auth_lost();

  bool m_initialized = false;
  std::unique_ptr<AuthListener> m_auth_listener;
};

}
