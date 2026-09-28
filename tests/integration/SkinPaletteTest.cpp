// setupPalette() used to reseed from qApp's palette, which the Skin itself
// had just written from the outgoing skin, so any role the incoming skin did
// not name kept the previous one's value.

#include <score_test/App.hpp>

#include <score/model/Skin.hpp>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPalette>
#include <QLabel>
#include <QImage>
#include <QPixmap>

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

TEST_CASE("Links are in the palette's orange unless the skin says otherwise", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto& skin = score::Skin::instance();
    // The default skin names no link colour: not Qt's dark blue, the orange
    skin.load(read_skin(":/skin/DefaultSkin.json"));
    CHECK(skin.WidgetPalette.color(QPalette::Link) == skin.WidgetPalette.color(QPalette::Light));
    CHECK(skin.WidgetPalette.color(QPalette::LinkVisited) == skin.WidgetPalette.color(QPalette::Light));
    // One that names its own keeps it
    skin.load(read_skin(":/skin/SolarizedDarkSkin.json"));
    CHECK(skin.WidgetPalette.color(QPalette::Link) == QColor(38, 139, 210));
    skin.load(read_skin(":/skin/DefaultSkin.json"));
  });
}

// A rich-text link, such as the library's "Explore the documentation", is not
// drawn in Qt's dark blue.
TEST_CASE("A link in a label is drawn in the skin's colour", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto& skin = score::Skin::instance();
    skin.load(read_skin(":/skin/DefaultSkin.json"));
    const QColor link = skin.WidgetPalette.color(QPalette::Link);

    QLabel label;
    label.setTextFormat(Qt::RichText);
    label.setText("<a href=\"https://ossia.io\">Explore the documentation</a>");
    label.resize(label.sizeHint());
    const QImage img = label.grab().toImage();

    int linkPixels = 0, bluePixels = 0;
    for(int y = 0; y < img.height(); y++)
      for(int x = 0; x < img.width(); x++)
      {
        const QColor c = img.pixelColor(x, y);
        if(c.blue() > c.red() + 60)
          bluePixels++;
        if(std::abs(c.red() - link.red()) < 40 && std::abs(c.green() - link.green()) < 40
           && std::abs(c.blue() - link.blue()) < 40)
          linkPixels++;
      }
    INFO("link " << link.name().toStdString());
    CHECK(bluePixels == 0);
    CHECK(linkPixels > 10);
  });
}

// A skin state saved in the settings may hold every palette role, Qt's own
// Link / LinkVisited too: those must not override the skin's link colour.
TEST_CASE("A saved skin state does not freeze Qt's link colours", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto& skin = score::Skin::instance();
    skin.load(read_skin(":/skin/DefaultSkin.json"));
    const QPalette fresh = skin.WidgetPalette;

    SECTION("a state saved before")
    {
      QJsonObject state = skin.toJson();
      QJsonObject pal = state["palette"].toObject();
      pal["Link"] = QJsonArray{0, 0, 255};
      pal["LinkVisited"] = QJsonArray{255, 0, 255};
      state["palette"] = pal;
      skin.load(state);
      CHECK(skin.WidgetPalette.color(QPalette::Link) == fresh.color(QPalette::Link));
      CHECK(
          skin.WidgetPalette.color(QPalette::LinkVisited)
          == fresh.color(QPalette::LinkVisited));
      CHECK(skin.WidgetPalette.color(QPalette::Link) != QColor(0, 0, 255));
    }

    SECTION("what is saved now")
    {
      const QJsonObject state = skin.toJson();
      const QJsonObject pal = state["palette"].toObject();
      // Only what differs from the built-in palette
      CHECK(!pal.contains("Link"));
      CHECK(!pal.contains("LinkVisited"));
      skin.load(state);
      checkSameAs(skin.WidgetPalette, fresh);

      // A role changed by the skin is still saved
      skin.load(read_skin(":/skin/NordSkin.json"));
      const QPalette nord = skin.WidgetPalette;
      const QJsonObject nordState = skin.toJson();
      CHECK(nordState["palette"].toObject().contains("Link"));
      skin.load(read_skin(":/skin/DefaultSkin.json"));
      skin.load(nordState);
      checkSameAs(skin.WidgetPalette, nord);
      skin.load(read_skin(":/skin/DefaultSkin.json"));
    }
  });
}
