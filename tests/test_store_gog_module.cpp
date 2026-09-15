#include "framework/nxtest.h"

#include "app/module_system/module.h"

TEST_CASE("store_gog: the module is in the build's registry") {
  bool found = false;
  for (const nxe::ModuleFactory factory : nxe::enabled_module_factories()) {
    const std::unique_ptr<nxe::Module> module = factory();
    found = found || (module != nullptr && module->name() == "store_gog");
  }
  CHECK(found);
}
