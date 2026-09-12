#include "store_gog/store_gog_config.h"
#include "store_gog/store_gog_platform.h"
#include "store_gog/store_gog_services.h"

#include "store/store_service.h"

#include "core/app/engine.h"
#include "core/app/module.h"
#include "core/app/module_context.h"

#include "core/foundation/diagnostics/log.h"

namespace nxm::store_gog {
namespace {

constexpr nx::string_view PUMP_SYSTEM = "store_gog.pump";
const nx::log::Category log_store_gog = nx::log::category("store_gog");

// store.iap is deliberately absent - the vendored GOG Galaxy SDK has no
// purchase/checkout API at all (confirmed absent from every header), only
// entitlement checks (store.core already covers those). A backend simply
// not registering a service it can't support is the same shape a missing
// cloud-saves/presence subsystem already has elsewhere in this module
// family - see store_gog_services.h.
constexpr nxe::ModuleService PROVIDED_SERVICES[] = {
    {.id = store::kCoreService, .version = {1, 0, 0}},
    {.id = store::kAchievementsService, .version = {1, 0, 0}},
    {.id = store::kCloudSavesService, .version = {1, 0, 0}},
    {.id = store::kPresenceService, .version = {1, 0, 0}},
};

class StoreGogModule final : public nxe::Module {
public:
  StoreGogModule()
      : m_core(m_platform), m_achievements(m_platform),
        m_cloud_saves(m_platform), m_presence(m_platform) {}

  [[nodiscard]] nxe::ModuleDescriptor descriptor() const noexcept override {
    nxe::ModuleDescriptor out{};
    out.id = "store_gog";
    out.version = {1, 0, 0};
    out.provided_services = PROVIDED_SERVICES;
    // The vendored Galaxy SDK ships Windows only (no macOS/Linux libraries
    // in this kit) - see NxStoreGog.cmake's FATAL_ERROR on !WIN32.
    out.platforms = nxe::ModulePlatform::Windows;
    return out;
  }

  bool on_register(nxe::ModuleContext &ctx) override {
    nxe::ServiceRegistrar registrar = ctx.service_registrar();
    store::StoreCore &core = m_core;
    store::StoreAchievements &achievements = m_achievements;
    store::StoreCloudSaves &cloud_saves = m_cloud_saves;
    store::StorePresence &presence = m_presence;
    return registrar.provide(store::kCoreService, PROVIDED_SERVICES[0].version, core) &&
           registrar.provide(store::kAchievementsService,
                              PROVIDED_SERVICES[1].version, achievements) &&
           registrar.provide(store::kCloudSavesService,
                              PROVIDED_SERVICES[2].version, cloud_saves) &&
           registrar.provide(store::kPresenceService, PROVIDED_SERVICES[3].version,
                              presence);
  }

  bool on_attach(nxe::ModuleContext &ctx) override {
    if (!ctx.schedule().try_define(
            PUMP_SYSTEM, nxe::sys::SystemFn([this](const nxe::sys::Context &) {
              m_platform.tick();
            }))) {
      nx::logw(log_store_gog, "system '{}' is already owned by another module",
                PUMP_SYSTEM);
      return false;
    }
    ctx.schedule().add(nxe::sys::Stage::Update, PUMP_SYSTEM);

    if (const std::optional<ServiceConfig> config = load_project_config();
        config.has_value()) {
      PlatformConfig platform_config;
      platform_config.client_id = config->client_id;
      platform_config.client_secret = config->client_secret;
      if (m_platform.initialize(platform_config))
        nx::logi(log_store_gog, "attached, Client ID {}", config->client_id);
      else
        nx::logw(log_store_gog, "Galaxy platform initialization failed");
    } else {
      nx::logi(log_store_gog, "no {} found; staying idle", kDefaultConfigPath);
    }

    return true;
  }

  void on_detach(nxe::ModuleContext &) override { m_platform.shutdown(); }

private:
  GogPlatform m_platform;
  GogCore m_core;
  GogAchievements m_achievements;
  GogCloudSaves m_cloud_saves;
  GogPresence m_presence;
};

} // namespace
} // namespace nxm::store_gog

NX_DECLARE_MODULE(store_gog, nxm::store_gog::StoreGogModule)
