// CLAP scan-cache tests:
//
//  * parseClapReply — one .clap file can host many plug-ins;
//  * resolveClapEntry — the macOS-bundle resolution that must run *before*
//    the known-paths check (it used to run after, so every startup rescanned
//    and re-appended every bundle: +1 duplicate per plug-in per launch);
//  * the end-to-end healing of a legacy cache polluted with hundreds of
//    duplicates by the fixed-port cross-instance bug, exercised through a
//    real application boot (legacy QVariant blob -> versioned cache).

#include <Clap/ApplicationPlugin.hpp>

#include <score_test/App.hpp>

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace
{
Clap::PluginInfo info(QString path, QString id, bool valid = true)
{
  Clap::PluginInfo i;
  i.path = std::move(path);
  i.id = std::move(id);
  i.name = valid ? "Some Plugin" : "<Invalid>";
  i.valid = valid;
  return i;
}
}

TEST_CASE("parseClapReply parses a multi-plugin file", "[pluginscan][clap]")
{
  const auto doc = QJsonDocument::fromJson(R"({
    "Path": "/claimed/elsewhere.clap",
    "Request": 1,
    "Token": "tok",
    "Plugins": [
      {"ID": "org.acme.comp", "Name": "Compressor", "Vendor": "ACME",
       "Features": ["audio-effect", "compressor"]},
      {"ID": "org.acme.limit", "Name": "Limiter", "Vendor": "ACME",
       "Features": ["audio-effect"]}
    ]
  })");
  REQUIRE(doc.isObject());

  const auto plugins = Clap::parseClapReply("/usr/lib/clap/acme.clap", doc.object());

  REQUIRE(plugins.size() == 2);
  CHECK(plugins[0].path == "/usr/lib/clap/acme.clap"); // scanned path wins
  CHECK(plugins[0].id == "org.acme.comp");
  CHECK(plugins[0].features == QList<QString>{"audio-effect", "compressor"});
  CHECK(plugins[0].valid);
  CHECK(plugins[1].id == "org.acme.limit");
}

TEST_CASE("parseClapReply: no Plugins array -> nothing", "[pluginscan][clap]")
{
  const auto doc
      = QJsonDocument::fromJson(R"({"Path":"/x.clap","Request":0,"Token":"t"})");
  CHECK(Clap::parseClapReply("/x.clap", doc.object()).empty());
}

TEST_CASE("resolveClapEntry resolves bundle directories to their binary", "[pluginscan][clap]")
{
  QTemporaryDir tmp;
  REQUIRE(tmp.isValid());

  SECTION("plain file stays as-is")
  {
    const QString file = tmp.path() + "/plain.clap";
    { QFile f{file}; REQUIRE(f.open(QIODevice::WriteOnly)); }
    CHECK(Clap::resolveClapEntry(file) == file);
  }
  SECTION("bundle directory resolves to Contents/MacOS/<name>")
  {
    const QString bundle = tmp.path() + "/Surge XT.clap";
    REQUIRE(QDir{}.mkpath(bundle + "/Contents/MacOS"));
    {
      QFile f{bundle + "/Contents/MacOS/Surge XT"};
      REQUIRE(f.open(QIODevice::WriteOnly));
    }
    CHECK(
        Clap::resolveClapEntry(bundle) == bundle + "/Contents/MacOS/Surge XT");
  }
  SECTION("Linux bundle-style dir without a macOS binary is skipped")
  {
    // Cardinal.clap on Linux: a *directory* containing Cardinal.clap,
    // CardinalFX.clap... which the recursive iteration finds by itself.
    // Resolving the directory to a nonexistent Contents/MacOS path used to
    // produce a spurious invalid-plug-in entry.
    const QString bundle = tmp.path() + "/Cardinal.clap";
    REQUIRE(QDir{}.mkpath(bundle));
    { QFile f{bundle + "/Cardinal.clap"}; REQUIRE(f.open(QIODevice::WriteOnly)); }
    CHECK(Clap::resolveClapEntry(bundle).isEmpty());
  }
}

TEST_CASE("a duplicate-ridden legacy CLAP cache is healed on boot", "[pluginscan][clap][heal]")
{
  qputenv("SCORE_DISABLE_AUDIOPLUGINS", "1");

  // Private settings location: ctest runs tests in parallel and QSettings
  // rewrites whole files, so sharing score-test.conf across processes would
  // be racy.
  static QTemporaryDir settings_dir;
  REQUIRE(settings_dir.isValid());
  QSettings::setPath(
      QSettings::NativeFormat, QSettings::UserScope, settings_dir.path());
  QSettings::setPath(
      QSettings::IniFormat, QSettings::UserScope, settings_dir.path());

  // Boot 1: seed a legacy-format cache the way the fixed-port bug left it:
  // every plug-in duplicated once per (cross-instance) run, plus a
  // valid/invalid pair from the timeout race. Seeding needs a booted app so
  // the QVariant stream operators are registered.
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    std::vector<Clap::PluginInfo> polluted;
    for(int run = 0; run < 50; run++)
    {
      polluted.push_back(info("/fake/surge.clap", "org.surge"));
      polluted.push_back(info("/fake/dexed.clap", "org.dexed"));
    }
    polluted.push_back(info("/fake/surge.clap", "", false)); // timeout marker
    polluted.push_back(info("/fake/dead.clap", "", false));  // genuinely broken

    QSettings s;
    s.remove("Effect/KnownCLAPCache");
    s.setValue("Effect/KnownCLAP", QVariant::fromValue(polluted));
  });

  // Boot 2: initialize() must migrate + heal.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = ctx.guiApplicationPlugin<Clap::ApplicationPlugin>();

    const auto& plugins = plug.plugins();
    REQUIRE(plugins.size() == 3);
    CHECK(plugins[0].path == "/fake/surge.clap");
    CHECK(plugins[0].valid);
    CHECK(plugins[1].path == "/fake/dexed.clap");
    CHECK(plugins[1].valid);
    CHECK(plugins[2].path == "/fake/dead.clap");
    CHECK(!plugins[2].valid);

    // Migration completed: legacy blob gone, versioned cache written
    QSettings s;
    CHECK(!s.contains("Effect/KnownCLAP"));
    CHECK(s.contains("Effect/KnownCLAPCache"));
  });

  // Boot 3: the healed cache survives a round-trip through the new format.
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = ctx.guiApplicationPlugin<Clap::ApplicationPlugin>();
    REQUIRE(plug.plugins().size() == 3);
    CHECK(plug.plugins()[0].id == "org.surge");
  });

  // Boot 4: a legacy blob rewritten by an *older* score after the migration
  // must not survive next to the versioned cache - it would resurrect stale
  // entries after a future format-version bump.
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QSettings s;
    REQUIRE(s.contains("Effect/KnownCLAPCache"));
    s.setValue(
        "Effect/KnownCLAP",
        QVariant::fromValue(std::vector<Clap::PluginInfo>{info("/stale.clap", "old")}));
  });
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto& plug = ctx.guiApplicationPlugin<Clap::ApplicationPlugin>();
    // Versioned cache won; the stale legacy entry is not loaded...
    REQUIRE(plug.plugins().size() == 3);
    // ...and the legacy key is gone again.
    QSettings s;
    CHECK(!s.contains("Effect/KnownCLAP"));
  });
}
