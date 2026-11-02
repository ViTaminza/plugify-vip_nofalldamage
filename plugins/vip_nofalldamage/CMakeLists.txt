# vip_nofalldamage — Plugify C++ plugin: port of the ABKAM module "[VIP] No Fall Damage".
#
# Pure s2sdk: no pisexcompat and no other wrapper library. The map gate (PITFALLS B1/B2) and the
# map-gated schema-offset cache (C8) the plugin needs live in its own src/ (plugin.cpp,
# schema_cache.hpp); only the official s2sdk, polyhook and vip bindings are linked.
#
#   plugify-vip_nofalldamage  the plugin shared library (entry "bin/vip_nofalldamage")
#   vip_nofalldamage_tests    engine-free unit tests: the block decision and the schema-offset
#                             cache policy (build/tests/vip_nofalldamage_tests)

set(VIP_NOFALLDAMAGE_VERSION "1.3.0")

# Binding globals the plugify module fills from the plugins declared as dependencies:
#   __s2sdk_*     (s2sdk_bindings)
#   __polyhook_*  (polyhook_bindings - the detour used for the damage hook the original got from
#                  the Pisex utils plugin)
#   __vip_*       (vip consumer bindings, generated into plugins/vip/include/external/plugify/include)
# In the monorepo plugins/vip is added first ("vip" < "vip_nofalldamage"), so vip_bindings already
# exists; a standalone exported repo only carries plugins/vip/include/ headers + vip.cpp and has no
# plugins/vip/CMakeLists.txt - define vip_bindings here when absent.
if(NOT TARGET vip_bindings)
    add_library(vip_bindings OBJECT
            "${CMAKE_SOURCE_DIR}/plugins/vip/include/external/plugify/include/vip/vip.cpp")
    target_include_directories(vip_bindings SYSTEM PUBLIC
            "${CMAKE_SOURCE_DIR}/plugins/vip/include/external/plugify/include"
            "${CMAKE_SOURCE_DIR}/sdk/include")
    target_compile_definitions(vip_bindings PUBLIC _GLIBCXX_USE_CXX11_ABI=1)
endif()

if(NOT TARGET polyhook_bindings)
    message(WARNING "vip_nofalldamage: polyhook_bindings is missing - the damage hook cannot be installed")
endif()

add_library(plugify-vip_nofalldamage SHARED
        src/plugin.cpp
        $<TARGET_OBJECTS:s2sdk_bindings>
        $<TARGET_OBJECTS:polyhook_bindings>
        $<TARGET_OBJECTS:vip_bindings>
)
target_include_directories(plugify-vip_nofalldamage PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# SYSTEM: warnings from the generated vip/s2sdk/plg/polyhook headers must not fail -Werror.
target_include_directories(plugify-vip_nofalldamage SYSTEM PRIVATE
        "${CMAKE_SOURCE_DIR}/plugins/vip/include"
        "${CMAKE_SOURCE_DIR}/plugins/vip/include/external/plugify/include"
        "${CMAKE_SOURCE_DIR}/sdk/include")
target_link_libraries(plugify-vip_nofalldamage PRIVATE
        plugify_sdk
        -static-libstdc++ -static-libgcc
)
target_compile_options(plugify-vip_nofalldamage PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(plugify-vip_nofalldamage PRIVATE
        _GLIBCXX_USE_CXX11_ABI=1
        PLUGIFY_MODULE="vip_nofalldamage"
        PLUGIFY_IS_DEBUG=0
        PLUGIFY_IS_RELEASE=1
)
set_target_properties(plugify-vip_nofalldamage PROPERTIES OUTPUT_NAME vip_nofalldamage)

if(UNIX AND NOT APPLE)
    target_link_options(plugify-vip_nofalldamage PRIVATE
            "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/sym/version_script.lds")
endif()

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/vip_nofalldamage.pplugin.in"
        "${CMAKE_CURRENT_BINARY_DIR}/vip_nofalldamage.pplugin"
)

# --- tests --------------------------------------------------------------------
# Engine-free logic tests, no s2sdk and no pisexcompat: the block decision of the original
# OnTakeDamage (damage_logic.hpp) and the map-gated schema-offset cache (schema_cache.hpp), both
# header-only. Run by ./test.sh (build/tests).
add_executable(vip_nofalldamage_tests tests/vip_nofalldamage_tests.cpp)
target_include_directories(vip_nofalldamage_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_options(vip_nofalldamage_tests PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(vip_nofalldamage_tests PRIVATE _GLIBCXX_USE_CXX11_ABI=1)
target_link_libraries(vip_nofalldamage_tests PRIVATE -static-libstdc++ -static-libgcc)
set_target_properties(vip_nofalldamage_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
