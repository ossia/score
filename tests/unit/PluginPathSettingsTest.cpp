// Per-format plug-in path settings:
//
//  * Vst3Paths / ClapPaths / Lv2Paths round-trip through the settings model
//    and notify;
//  * each backend rescans on *its own* path setting;
//  * regression: VST3 used to listen to the VST2 path setting (a `//! TODO`
//    left from the unfinished settings refactor) and wiped its whole
//    database whenever the VST2 paths changed.

#include <Clap/ApplicationPlugin.hpp>
#include <Media/Effect/Settings/Model.hpp>
#include <Vst3/ApplicationPlugin.hpp>

#include <score_test/App.hpp>

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("plug-in path settings round-trip and notify", "[pluginscan][settings]")
{
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& m = ctx.settings<Media::Settings::Model>();

    // Baseline first: settings persist across test runs, and set() is a
    // no-op (no notification) when the value does not change
    m.setVst3Paths({"/baseline"});
    m.setClapPaths({"/baseline"});
    m.setLv2Paths({"/baseline"});

    int vst3_notified{}, clap_notified{}, lv2_notified{};
    QObject::connect(
        &m, &Media::Settings::Model::Vst3PathsChanged, &m,
        [&](const QStringList&) { vst3_notified++; });
    QObject::connect(
        &m, &Media::Settings::Model::ClapPathsChanged, &m,
        [&](const QStringList&) { clap_notified++; });
    QObject::connect(
        &m, &Media::Settings::Model::Lv2PathsChanged, &m,
        [&](const QStringList&) { lv2_notified++; });

    m.setVst3Paths({"/some/vst3/dir"});
    m.setClapPaths({"/some/clap/dir"});
    m.setLv2Paths({"/some/lv2/dir"});

    CHECK(m.getVst3Paths() == QStringList{"/some/vst3/dir"});
    CHECK(m.getClapPaths() == QStringList{"/some/clap/dir"});
    CHECK(m.getLv2Paths() == QStringList{"/some/lv2/dir"});
    CHECK(vst3_notified == 1);
    CHECK(clap_notified == 1);
    CHECK(lv2_notified == 1);

    // Same value again: no spurious notification (and no spurious rescan)
    m.setVst3Paths({"/some/vst3/dir"});
    CHECK(vst3_notified == 1);
  });
}

TEST_CASE("VST3 rescans on its own path setting, not on VST2's", "[pluginscan][settings]")
{
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& m = ctx.settings<Media::Settings::Model>();
    auto& vst3 = ctx.applicationPlugin<vst3::ApplicationPlugin>();

    QTemporaryDir empty_dir;
    REQUIRE(empty_dir.isValid());

    // Fake cached VST3 so we can observe what a rescan does to it
    vst3::AvailablePlugin fake;
    fake.path = "/fake/plugin.vst3";
    fake.name = "Fake";
    fake.isValid = false;
    vst3.vst_infos.push_back(fake);

    // Changing the *VST2* paths must not touch the VST3 database.
    // (It used to clear it and trigger a full rescan.)
    m.setVstPaths({empty_dir.path()});
    REQUIRE(vst3.vst_infos.size() == 1);

    // Changing the VST3 paths does rescan: the fake entry does not exist on
    // disk in the new paths, so it gets pruned.
    m.setVst3Paths({empty_dir.path()});
    CHECK(vst3.vst_infos.empty());
  });
}

TEST_CASE("CLAP rescans on its path setting", "[pluginscan][settings]")
{
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& m = ctx.settings<Media::Settings::Model>();
    auto& clap = ctx.guiApplicationPlugin<Clap::ApplicationPlugin>();

    QTemporaryDir empty_dir;
    REQUIRE(empty_dir.isValid());
    m.setClapPaths({"/baseline"}); // ensure the next set is a real change

    int changed{};
    QObject::connect(
        &clap, &Clap::ApplicationPlugin::pluginsChanged, &clap, [&] { changed++; });

    // Sanity: direct emission is observable across the plug-in boundary
    clap.pluginsChanged();
    REQUIRE(changed == 1);

    m.setClapPaths({empty_dir.path()});
    // rescanPlugins() ran: it signals at least once after pruning
    CHECK(changed >= 2);
  });
}
