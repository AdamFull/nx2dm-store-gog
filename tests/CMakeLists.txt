
add_executable(nx_store_gog_tests
        test_store_gog_config.cpp
        test_store_gog_module.cpp
        test_store_gog_services.cpp
        ../store_gog_config.cpp
        ../store_gog_module.cpp
        ../store_gog_platform.cpp
        ../store_gog_services.cpp)

nx_module_registry(nx_store_gog_tests)

target_include_directories(nx_store_gog_tests PRIVATE
        "${NX_ENGINE_ROOT}/engine" "${NX_ENGINE_ROOT}/modules")

target_link_libraries(nx_store_gog_tests
        PRIVATE nx::test nx::foundation nx::engine nx::galaxysdk nx::compile_options)

set_target_properties(nx_store_gog_tests PROPERTIES FOLDER "tests")
nxtest_discover_tests(nx_store_gog_tests LABELS store_gog)
