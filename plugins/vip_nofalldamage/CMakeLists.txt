# vip_nofalldamage — Plugify C++ plugin: port of the ABKAM module "[VIP] No Fall Damage".
#
#   plugify-vip_nofalldamage  the plugin shared library (entry "bin/vip_nofalldamage")

set(VIP_NOFALLDAMAGE_VERSION "1.2.1")

# Binding globals the plugify module fills from the plugins declared as dependencies:
#   __s2sdk_* / __hudmenu_* (s2sdk_bindings, hudmenu_bindings - both come in through
#   pisexcompat's PUBLIC link), __vip_* (vip_bindings) and __polyhook_* (polyhook_bindings, the
#   detour used for the damage hook the original got from the Pisex utils plugin).
set(VIP_NOFALLDAMAGE_BINDING_OBJECTS "")
if(TARGET vip_bindings)
    list(APPEND VIP_NOFALLDAMAGE_BINDING_OBJECTS $<TARGET_OBJECTS:vip_bindings>)
else()
    message(WARNING "vip_nofalldamage: vip_bindings is missing (build plugins/vip first)")
endif()
if(NOT TARGET polyhook_bindings)
    message(WARNING "vip_nofalldamage: polyhook_bindings is missing - the damage hook cannot be installed")
endif()

add_library(plugify-vip_nofalldamage SHARED
        src/plugin.cpp
        $<TARGET_OBJECTS:s2sdk_bindings>
        $<TARGET_OBJECTS:polyhook_bindings>
        ${VIP_NOFALLDAMAGE_BINDING_OBJECTS}
)
target_include_directories(plugify-vip_nofalldamage PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
# SYSTEM: warnings from the generated vip/s2sdk/plg/polyhook/pisexcompat headers must not fail -Werror.
target_include_directories(plugify-vip_nofalldamage SYSTEM PRIVATE
        "${CMAKE_SOURCE_DIR}/plugins/vip/include"
        "${CMAKE_SOURCE_DIR}/plugins/vip/include/external/plugify/include"
        "${CMAKE_SOURCE_DIR}/sdk/include"
        "${CMAKE_SOURCE_DIR}/libs/pisexcompat/include")
target_link_libraries(plugify-vip_nofalldamage PRIVATE
        pisexcompat
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
# Engine-free logic tests; run by ./test.sh (build/tests).
add_executable(vip_nofalldamage_tests tests/vip_nofalldamage_tests.cpp)
target_include_directories(vip_nofalldamage_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_options(vip_nofalldamage_tests PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(vip_nofalldamage_tests PRIVATE _GLIBCXX_USE_CXX11_ABI=1)
target_link_libraries(vip_nofalldamage_tests PRIVATE -static-libstdc++ -static-libgcc)
set_target_properties(vip_nofalldamage_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
