// Gfx/Spout (Windows) and Gfx/Syphon (macOS): the two shared-texture device
// families. Neither compiles on Linux, so nothing in tests/ has ever named
// them and no CI job would notice if their registration broke.
//
// This target builds on EVERY platform. What varies is what it asserts:
//
//   * On the platform that supports the family (GFX_TEST_HAS_SPOUT /
//     GFX_TEST_HAS_SYPHON, set by the same CMake condition the plugin itself
//     uses), the four protocol factories must be registered, must have a
//     pretty name and a default DeviceSettings, and must produce a settings
//     widget whose round trip preserves the device name.
//
//   * Everywhere else the test SKIPs with the reason stated — and, before it
//     skips, asserts the *negative*: the factory must NOT be registered on a
//     platform that cannot implement it. That half runs on Linux, so the file
//     is not dead weight here; it would catch a stray registration tomorrow.
//
// Everything goes through Device::ProtocolFactoryList by UUID: the Spout and
// Syphon classes are not exported from the shared plug-in, and adding an
// export for a Windows-only type just to name it in a test would be worse than
// looking it up the way the application does.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace score::test;

namespace
{
// Gfx/Spout/SpoutInput.hpp, Gfx/Spout/SpoutOutput.hpp
constexpr auto UUID_SPOUT_IN = "3c995cb6-052b-4c52-a8fd-841b33b81b29";
constexpr auto UUID_SPOUT_OUT = "ddf45db7-9eaf-453c-8fc0-86ccdf21677c";
// Gfx/Syphon/SyphonInput.hpp, Gfx/Syphon/SyphonOutput.hpp
constexpr auto UUID_SYPHON_IN = "398CEC01-C4EA-43B7-8281-D848748E0F68";
constexpr auto UUID_SYPHON_OUT = "087D032D-9A42-4BC9-B3DF-AD9BA9E86C07";

Device::ProtocolFactory*
protocol(const score::GUIApplicationContext& ctx, const char* uuid)
{
  auto& list = ctx.interfaces<Device::ProtocolFactoryList>();
  return list.get(
      UuidKey<Device::ProtocolFactory>::fromString(QString::fromUtf8(uuid)));
}

/// The contract every shared-texture protocol factory has to satisfy: a name,
/// usable default settings that carry its own key, and a settings widget that
/// gives those settings back unchanged.
void check_shared_texture_factory(
    const score::GUIApplicationContext& ctx, const char* uuid, const char* what)
{
  INFO(what);
  auto* f = protocol(ctx, uuid);
  REQUIRE(f != nullptr);

  CHECK_FALSE(f->prettyName().isEmpty());

  const auto& def = f->defaultSettings();
  CHECK(def.protocol == f->concreteKey());
  CHECK_FALSE(def.name.isEmpty());

  auto* w = f->makeSettingsWidget();
  REQUIRE(w != nullptr);
  w->setSettings(def);
  const auto back = w->getSettings();
  CHECK(back.protocol == def.protocol);
  CHECK(back.name == def.name);
  delete w;
}
}

TEST_CASE("Spout devices are registered on Windows only", "[gfx][spout][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
#if defined(GFX_TEST_HAS_SPOUT)
    check_shared_texture_factory(ctx, UUID_SPOUT_IN, "Spout input");
    check_shared_texture_factory(ctx, UUID_SPOUT_OUT, "Spout output");
#else
    // The negative half: nothing may claim these keys off Windows.
    CHECK(protocol(ctx, UUID_SPOUT_IN) == nullptr);
    CHECK(protocol(ctx, UUID_SPOUT_OUT) == nullptr);
    SKIP("Spout is Windows/x86_64 only (SCORE_HAS_SPOUT); the negative "
         "registration check above did run");
#endif
  });
}

TEST_CASE("Syphon devices are registered on macOS only", "[gfx][syphon][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
#if defined(GFX_TEST_HAS_SYPHON)
    check_shared_texture_factory(ctx, UUID_SYPHON_IN, "Syphon input");
    check_shared_texture_factory(ctx, UUID_SYPHON_OUT, "Syphon output");
#else
    CHECK(protocol(ctx, UUID_SYPHON_IN) == nullptr);
    CHECK(protocol(ctx, UUID_SYPHON_OUT) == nullptr);
    SKIP("Syphon is macOS only (APPLE); the negative registration check above "
         "did run");
#endif
  });
}

TEST_CASE(
    "Exactly one shared-texture family is available per platform",
    "[gfx][spout][syphon][gui]")
{
  // Spout and Syphon are mutually exclusive by construction. A build that
  // registered both, or a Linux build that registered either, means the CMake
  // condition drifted from the #if in score_plugin_gfx.cpp.
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    const bool spout = protocol(ctx, UUID_SPOUT_OUT) != nullptr;
    const bool syphon = protocol(ctx, UUID_SYPHON_OUT) != nullptr;
    CHECK_FALSE((spout && syphon));

#if defined(GFX_TEST_HAS_SPOUT)
    CHECK(spout);
#elif defined(GFX_TEST_HAS_SYPHON)
    CHECK(syphon);
#else
    CHECK_FALSE(spout);
    CHECK_FALSE(syphon);
#endif
  });
}
