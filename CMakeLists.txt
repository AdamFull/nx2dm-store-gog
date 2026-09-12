
option(NX_MODULE_STORE_GOG "Build the GOG Galaxy storefront backend" OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/NxStoreGog.cmake")

if (NOT NX_MODULE_STORE_GOG)
    return()
endif ()

# nx_module()'s own PLATFORMS gate runs too late to help here - a project
# that also builds for Android/iOS still passes -DNX_MODULE_STORE_GOG=ON
# for this platform-irrelevant module (its module list isn't per-platform),
# so the vendor-SDK resolution below must skip itself before ever running,
# the same "skipped on <platform>" outcome _nx_module_gate would otherwise
# report - just early enough to not FATAL_ERROR chasing a Windows-only SDK
# on a platform that was never going to build this module anyway.
_nx_current_platform(_nx_store_gog_here)
if (NOT _nx_store_gog_here STREQUAL "windows")
    message(STATUS "nx2d: module 'store_gog' skipped on ${_nx_store_gog_here}")
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
