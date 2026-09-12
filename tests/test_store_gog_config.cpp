#include "framework/nxtest.h"

#include "store_gog/store_gog_config.h"

#include "core/foundation/vfs/vfs.h"

namespace {

using nxm::store_gog::load_project_config;

struct VfsScope {
  VfsScope() { initialized = nx::vfs::initialize(); }
  ~VfsScope() { nx::vfs::shutdown(); }
  bool initialized = false;
};

[[nodiscard]] nx::blob<u8> as_bytes(const nx::string_view text) {
  return nx::blob<u8>(
      {reinterpret_cast<const u8 *>(text.data()), text.size()});
}

constexpr nx::string_view kValid = R"(
[store_gog]
client_id = client-1
client_secret = secret-1
)";

} // namespace

TEST_CASE("store_gog config: no file present is not an error") {
  const VfsScope scope;
  REQUIRE(scope.initialized);
  nx::vfs::MemoryDevice *const memory = nx::vfs::make_memory_device();
  REQUIRE(memory != nullptr);
  const nx::vfs::MountId mount = nx::vfs::mount("/", memory);

  CHECK_FALSE(load_project_config().has_value());

  nx::vfs::unmount(mount);
}

TEST_CASE("store_gog config: a valid file is parsed in full") {
  const VfsScope scope;
  REQUIRE(scope.initialized);
  nx::vfs::MemoryDevice *const memory = nx::vfs::make_memory_device();
  REQUIRE(memory != nullptr);
  memory->add("/config/store_gog.ini", as_bytes(kValid));
  const nx::vfs::MountId mount = nx::vfs::mount("/", memory);

  const std::optional<nxm::store_gog::ServiceConfig> config =
      load_project_config();
  REQUIRE(config.has_value());
  CHECK(config->client_id.view() == "client-1");
  CHECK(config->client_secret.view() == "secret-1");

  nx::vfs::unmount(mount);
}

TEST_CASE("store_gog config: any missing required field is refused") {
  const VfsScope scope;
  REQUIRE(scope.initialized);
  nx::vfs::MemoryDevice *const memory = nx::vfs::make_memory_device();
  REQUIRE(memory != nullptr);
  memory->add("/config/store_gog.ini",
             as_bytes("[store_gog]\nclient_id = client-1\n"));
  const nx::vfs::MountId mount = nx::vfs::mount("/", memory);

  CHECK_FALSE(load_project_config().has_value());

  nx::vfs::unmount(mount);
}

TEST_CASE("store_gog config: malformed ini does not crash") {
  const VfsScope scope;
  REQUIRE(scope.initialized);
  nx::vfs::MemoryDevice *const memory = nx::vfs::make_memory_device();
  REQUIRE(memory != nullptr);
  memory->add("/config/store_gog.ini", as_bytes("this is not [ini at all"));
  const nx::vfs::MountId mount = nx::vfs::mount("/", memory);

  CHECK_FALSE(load_project_config().has_value());

  nx::vfs::unmount(mount);
}

TEST_CASE("store_gog config: an explicit path overrides the default") {
  const VfsScope scope;
  REQUIRE(scope.initialized);
  nx::vfs::MemoryDevice *const memory = nx::vfs::make_memory_device();
  REQUIRE(memory != nullptr);
  memory->add("/somewhere/else.ini", as_bytes(kValid));
  const nx::vfs::MountId mount = nx::vfs::mount("/", memory);

  CHECK_FALSE(load_project_config().has_value());
  const std::optional<nxm::store_gog::ServiceConfig> config =
      load_project_config("/somewhere/else.ini");
  REQUIRE(config.has_value());
  CHECK(config->client_id.view() == "client-1");

  nx::vfs::unmount(mount);
}
