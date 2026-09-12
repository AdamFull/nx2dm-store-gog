#include "store_gog/store_gog_platform.h"

#include "core/foundation/diagnostics/log.h"

namespace nxm::store_gog {
namespace {

const nx::log::Category log_store_gog = nx::log::category("store_gog");

}

class GogPlatform::AuthListener final : public galaxy::api::IAuthListener {
public:
  explicit AuthListener(GogPlatform &platform) noexcept : m_platform(platform) {}

  void OnAuthSuccess() override { m_platform.on_auth_success(); }
  void OnAuthFailure(const FailureReason failure_reason) override {
    m_platform.on_auth_failure(static_cast<int>(failure_reason));
  }
  void OnAuthLost() override { m_platform.on_auth_lost(); }

private:
  GogPlatform &m_platform;
};

GogPlatform::GogPlatform() noexcept = default;
GogPlatform::~GogPlatform() = default;

bool GogPlatform::initialize(const PlatformConfig &config) {
  const galaxy::api::InitOptions options(config.client_id.c_str(),
                                          config.client_secret.c_str());
  galaxy::api::Init(options);
  if (const galaxy::api::IError *const error = galaxy::api::GetError();
      error != nullptr) {
    nx::logw(log_store_gog, "galaxy::api::Init failed: {}", error->GetMsg());
    return false;
  }
  m_initialized = true;

  // A one-shot listener passed directly to SignInGalaxy(), not a
  // self-registering one - but OnAuthLost() can fire at any later point
  // after success, so it still needs to outlive this call, not just the
  // first callback; kept as a member for the platform's whole lifetime.
  m_auth_listener = std::make_unique<AuthListener>(*this);
  galaxy::api::User()->SignInGalaxy(false, 15, m_auth_listener.get());
  return true;
}

void GogPlatform::tick() {
  if (m_initialized)
    galaxy::api::ProcessData();
}

bool GogPlatform::logged_on() const noexcept {
  return m_initialized && galaxy::api::User() != nullptr &&
         galaxy::api::User()->IsLoggedOn();
}

void GogPlatform::shutdown() {
  if (m_initialized) {
    galaxy::api::Shutdown();
    m_initialized = false;
  }
  m_auth_listener.reset();
}

void GogPlatform::on_auth_success() {
  nx::logi(log_store_gog, "Galaxy sign-in succeeded");
}

void GogPlatform::on_auth_failure(const int failure_reason) {
  // Expected whenever no GOG Galaxy Client is installed/running, or the
  // signed-in user has no license for this game - not an error, the same
  // honest-failure shape store_steam's SteamAPI_Init() already has when no
  // Steam client is running.
  nx::logi(log_store_gog, "Galaxy sign-in failed ({}) - staying idle",
            failure_reason);
}

void GogPlatform::on_auth_lost() {
  nx::logw(log_store_gog, "Galaxy sign-in lost");
}

}
