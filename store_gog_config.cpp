#include "store_gog/store_gog_config.h"

#include "core/foundation/diagnostics/log.h"
#include "core/foundation/serialization/ini.h"
#include "core/foundation/vfs/vfs.h"

namespace nxm::store_gog {
namespace {

const nx::log::Category log_store_gog = nx::log::category("store_gog");

[[nodiscard]] const nx::string *
require(const nx::ini::Document &doc, const nx::string_view key,
        const nx::string_view path, bool &ok) {
  const nx::string *const value = doc.find("store_gog", key);
  if (value == nullptr) {
    nx::logw(log_store_gog, "config: '{}' is missing [store_gog] {}", path,
              key);
    ok = false;
  }
  return value;
}

} // namespace

std::optional<ServiceConfig> load_project_config(const nx::string_view path) {
  const auto text = nx::vfs::read_text(path);
  if (!text) {
    if (text.error().kind != nx::fs::io_error::NotFound)
      nx::logw(log_store_gog, "config: could not read '{}': {}", path,
                nx::fs::to_string(text.error().kind));
    return {};
  }

  const auto parsed = nx::ini::parse(text->view());
  if (!parsed) {
    const nx::ini::ParseError &error = parsed.error();
    nx::logw(log_store_gog, "config: {}:{}:{}: {}", path, error.line,
              error.column, error.message());
    return {};
  }

  bool ok = true;
  const nx::string *const client_id = require(*parsed, "client_id", path, ok);
  const nx::string *const client_secret =
      require(*parsed, "client_secret", path, ok);
  if (!ok)
    return {};

  ServiceConfig config;
  config.client_id = *client_id;
  config.client_secret = *client_secret;
  return config;
}

}
