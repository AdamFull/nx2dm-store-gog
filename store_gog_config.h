#pragma once

#include "core/foundation/strings/utf8_string.h"

#include <optional>

namespace nxm::store_gog {

/// VFS path StoreGogModule::on_attach() checks automatically. A project
/// enables the module by cooking a file to this path (author it at
/// assets/config/store_gog.ini) with its game's GOG Galaxy credentials from
/// the GOG Developer Portal - e.g.:
///
///   [store_gog]
///   client_id = ...
///   client_secret = ...
///
/// Both are required. client_secret is a real secret - this file likely
/// shouldn't be committed to a public repository.
inline constexpr nx::string_view kDefaultConfigPath = "/config/store_gog.ini";

struct ServiceConfig {
  nx::string client_id;
  nx::string client_secret;
};

/// Reads and validates an INI file at @p path. Empty if the file does not
/// exist, or (with a logged warning) if it exists but fails to parse or is
/// missing a required field - callers should treat both the same way, as
/// "nothing to auto-configure with".
[[nodiscard]] std::optional<ServiceConfig>
load_project_config(nx::string_view path = kDefaultConfigPath);

}
