
# The GOG Galaxy SDK is a licence-gated download from GOG's developer portal -
# there is no fetchable URL, so it stays a required local, gitignored,
# developer-provided directory, the same shape modules/store_steam and
# modules/store_egs already established.

set(NX_STORE_GOG_SDK_DIR "" CACHE PATH
        "An extracted GOG Galaxy SDK (its 'DevelopmentKit_<version>_...' directory, or that directory itself). Empty uses modules/store_gog/third_party/GogSdk.")

function(nx_add_galaxy_sdk)
    if (NX_STORE_GOG_SDK_DIR)
        set(_search "${NX_STORE_GOG_SDK_DIR}")
    else ()
        set(_search "${CMAKE_CURRENT_LIST_DIR}/third_party/GogSdk")
    endif ()

    _nx_resolve_vendor_root("${_search}" "Include/galaxy/GalaxyApi.h" _root)
    if (NOT _root)
        message(FATAL_ERROR
                "nx2d: no GOG Galaxy SDK. Download it (GOG developer "
                "account required) from https://devportal.gog.com, then "
                "extract it to modules/store_gog/third_party/GogSdk (or "
                "point NX_STORE_GOG_SDK_DIR at it). It is not fetchable "
                "here: GOG distributes it only to registered developers. "
                "Never commit it - modules/store_gog/third_party/GogSdk is "
                "gitignored on purpose.")
    endif ()

    if (NOT WIN32)
        message(FATAL_ERROR "nx2d: this GOG Galaxy SDK drop is Windows-only (the vendored kit ships no macOS/Linux libraries)")
    endif ()
    if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "nx2d: the vendored GOG Galaxy SDK is 64-bit only")
    endif ()

    set(_lib "${_root}/Libraries")
    _nx_imported_shared_library(nx_galaxysdk
            RUNTIME "${_lib}/Galaxy64.dll"
            IMPLIB "${_lib}/Galaxy64.lib")
    add_library(nx::galaxysdk ALIAS nx_galaxysdk)
    set_target_properties(nx_galaxysdk PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${_root}/Include")

    message(STATUS "nx2d: GOG Galaxy SDK from ${_root}")
endfunction()
