// setupPalette() used to reseed from qApp's palette, which the Skin itself
// had just written from the outgoing skin, so any role the incoming skin did
// not name kept the previous one's value.

#include <score_test/App.hpp>

#include <score/model/Skin.hpp>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>

#include <catch2/catch_test_macros.hpp>

namespace
{
QJsonObject read_skin(const QString& path)
{
  QFile f{path};
  REQUIRE(f.open(QFile::ReadOnly));
  return QJsonDocument::fromJson(f.readAll()).object();
}

//! Every role in every group, so a leak anywhere is reported by name.
void checkSameAs(const QPalette& got, const QPalette& want)
{
  for(auto& [key, role] : score::Skin::paletteRoles())
  {
    for(auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled})
    {
      INFO("role " << key << ", group " << int(group));
      CHECK(got.brush(group, role).color() == want.brush(group, role).color());
    }
  }
}
}

TEST_CASE("A skin switch leaves no palette role behind", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto& skin = score::Skin::instance();

    skin.load(read_skin(":/skin/DefaultSkin.json"));
    const QPalette fresh = skin.WidgetPalette;

    // Nord names BrightText, Dark, Shadow and a disabled block; Default names
    // none of them, so a leak shows up on the way back.
    skin.load(read_skin(":/skin/NordSkin.json"));
    REQUIRE(skin.WidgetPalette != fresh);

    skin.load(read_skin(":/skin/DefaultSkin.json"));
    checkSameAs(skin.WidgetPalette, fresh);
  });
}

TEST_CASE("Every shipped skin returns to the same palette", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto& skin = score::Skin::instance();
    const QJsonObject def = read_skin(":/skin/DefaultSkin.json");

    skin.load(def);
    const QPalette fresh = skin.WidgetPalette;

    QDirIterator it{":/skin", {"*.json"}, QDir::Files};
    int n = 0;
    while(it.hasNext())
    {
      const QString path = it.next();
      INFO("after " << path.toStdString());
      skin.load(read_skin(path));
      skin.load(def);
      checkSameAs(skin.WidgetPalette, fresh);
      n++;
    }
    CHECK(n > 30);
  });
}

TEST_CASE("A palette round-trip keeps every group", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto& skin = score::Skin::instance();

    skin.load(read_skin(":/skin/NordSkin.json"));
    const QPalette before = skin.WidgetPalette;

    const QJsonObject saved = skin.toJson();
    skin.load(read_skin(":/skin/DefaultSkin.json"));
    skin.load(saved);

    checkSameAs(skin.WidgetPalette, before);
  });
}
