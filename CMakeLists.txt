
option(NX_MODULE_STORE_GOG "Build the GOG Galaxy storefront backend" OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/NxStoreGog.cmake")

if (NOT NX_MODULE_STORE_GOG)
    return()
endif ()

nx_add_galaxy_sdk()

nx_module(store_gog
        DISPLAY "GOG Galaxy storefront backend"
        SOURCES store_gog_module.cpp store_gog_config.cpp store_gog_platform.cpp
                store_gog_services.cpp
        DEPENDS nx::galaxysdk nx_module_store
        PLATFORMS windows)

if (NX_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
