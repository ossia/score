// Process::Pixmaps used to build every pixmap once, from the skin and the
// device pixel ratio in force at first use, and never again.

#include <score_test/App.hpp>

#include <score/model/Skin.hpp>

#include <Process/Style/Pixmaps.hpp>

#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("The skin-drawn pixmaps follow the skin", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto read = [](const char* path) {
      QFile f{QString::fromUtf8(path)};
      REQUIRE(f.open(QFile::ReadOnly));
      return QJsonDocument::fromJson(f.readAll()).object();
    };
    auto& skin = score::Skin::instance();

    skin.load(read(":/skin/DefaultSkin.json"));
    const QImage dark = Process::Pixmaps::instance().metricHandle.toImage();
    REQUIRE(!dark.isNull());

    // Solarized Light's Gray is (141,155,155) against Default's (127,127,127).
    skin.load(read(":/skin/SolarizedLightSkin.json"));
    const QImage light = Process::Pixmaps::instance().metricHandle.toImage();

    CHECK(dark != light);

    skin.load(read(":/skin/DefaultSkin.json"));
    CHECK(Process::Pixmaps::instance().metricHandle.toImage() == dark);
  });
}
