// Skin files carry fonts, not just colours, so that a skin can retarget the
// whole UI at a different family and size. This covers the three things that
// can silently go wrong:
//
//  * a skin's "fonts" block must actually reach the Skin members;
//  * loading a second skin must not inherit the first skin's fonts, which is
//    why Skin::load() resets to the built-in defaults first;
//  * toJson() must write back what load() reads, or the Save button in the
//    theme editor quietly drops the fonts.
//
// The pixel sizes asserted here are the design grids of the shipped pixel
// fonts. Off-grid sizes render with uneven stem widths, so the numbers are
// part of the contract, not arbitrary.

#include <score_test/App.hpp>

#include <score/model/Skin.hpp>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFontDatabase>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace
{
QJsonObject read_skin(const QString& path)
{
  QFile f(path);
  REQUIRE(f.open(QFile::ReadOnly));
  QJsonParseError err{};
  const auto doc = QJsonDocument::fromJson(f.readAll(), &err);
  REQUIRE(err.error == QJsonParseError::NoError);
  return doc.object();
}
}

TEST_CASE("A skin file drives the fonts", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::Skin& skin = score::Skin::instance();

    const QJsonObject small = read_skin(QStringLiteral(":/skin/SmallScreenSkin.json"));
    REQUIRE(small.contains("fonts"));

    skin.load(small);

    // The small-screen skin puts the body on Galmuri9's 10 px grid.
    CHECK(skin.SansFont.families().value(0) == "Galmuri9");
    CHECK(skin.SansFont.pixelSize() == 10);
    CHECK(skin.MonoFont.families().value(0) == "GalmuriMono9");
    CHECK(skin.MonoFont.pixelSize() == 10);
    CHECK(skin.ApplicationFont.families().value(0) == "Galmuri9");
    CHECK(skin.ApplicationFont.pixelSize() == 10);

    // Smaller and larger roles move to the fonts drawn for those grids,
    // rather than scaling one font off its own grid.
    CHECK(skin.SansFontSmall.families().value(0) == "Galmuri7");
    CHECK(skin.SansFontSmall.pixelSize() == 8);
    CHECK(skin.Medium7Pt.families().value(0) == "Galmuri7");
    CHECK(skin.Medium7Pt.pixelSize() == 8);
    CHECK(skin.TitleFont.families().value(0) == "Galmuri11");
    CHECK(skin.TitleFont.pixelSize() == 12);

    // "antialias": false in the skin's defaults must reach the style
    // strategy, since that is what actually keeps the pixels square.
    CHECK((int(skin.SansFont.styleStrategy()) & int(QFont::NoAntialias)) != 0);

    // Galmuri11-Bold shares the "Galmuri11" family, so it is selected by
    // style name. Losing this would silently give a smeared fake bold.
    // Bold10Pt lands on Galmuri9, which has no Bold face, so the skin must
    // not ask for one: Qt would fake it by smearing the glyphs and the label
    // would come out with ragged 2 px stems.
    CHECK(skin.Bold10Pt.families().value(0) == "Galmuri9");
    CHECK(skin.Bold10Pt.styleName().isEmpty());
    CHECK(skin.Bold10Pt.bold() == false);

    // The title role does have a real bold, and asks for it by name.
    CHECK(skin.TitleFont.styleName() == "Bold");

    SECTION("loading another skin does not inherit the previous fonts")
    {
      skin.load(read_skin(QStringLiteral(":/skin/DefaultSkin.json")));

      CHECK(skin.SansFont.families().value(0) != "Galmuri9");
      CHECK(skin.MonoFont.families().value(0) != "GalmuriMono9");
      CHECK(skin.TitleFont.families().value(0) != "Galmuri11");
      CHECK(skin.TitleFont.styleName() != "Bold");
    }

    SECTION("toJson round-trips through load")
    {
      const QJsonObject saved = skin.toJson();
      REQUIRE(saved.contains("fonts"));

      // A saved skin must reload identically, otherwise the theme editor's
      // Save loses the fonts it just wrote.
      skin.load(saved);
      CHECK(skin.toJson()["fonts"] == saved["fonts"]);

      // And it must still be the small-screen configuration.
      CHECK(skin.SansFont.families().value(0) == "Galmuri9");
      CHECK(skin.SansFont.pixelSize() == 10);
    }

    SECTION("a skin with no fonts block keeps the built-in fonts")
    {
      QJsonObject noFonts = small;
      noFonts.remove("fonts");
      skin.load(noFonts);

      CHECK(skin.SansFont.families().value(0) != "Galmuri9");
      // The built-in SansFont is deliberately left on its point size, so
      // pixelSize() is -1 here. Saving must not invent a pixel size for it;
      // that is what the round-trip section above guards.
      CHECK(skin.SansFont.pixelSize() == -1);
    }
  });
}

TEST_CASE("The shipped pixel fonts are registered", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // A skin naming a font score does not ship would fall back to a smooth
    // font and look broken, so check the families the skins reference.
    // The fixture builds a fresh application per test, and the font database
    // is per-application, so ask for the fonts here rather than relying on
    // whichever earlier test happened to construct the Skin.
    score::registerApplicationFonts();

    const QStringList families = QFontDatabase::families();
    for(const char* f : {"Galmuri7", "Galmuri9", "Galmuri11", "Galmuri14",
                         "GalmuriMono7", "GalmuriMono9", "GalmuriMono11",
                         "Departure Mono", "Cozette", "CozetteVector"})
    {
      CHECK(families.contains(QString::fromUtf8(f)));
    }

    // Galmuri11's Bold and Condensed are faces of the one family.
    const QStringList styles = QFontDatabase::styles(QStringLiteral("Galmuri11"));
    CHECK(styles.contains(QStringLiteral("Bold")));
    CHECK(styles.contains(QStringLiteral("Condensed")));
  });
}

TEST_CASE("Font sizes come from the skin, not from a setting", "[integration][skin]")
{
  // Sizes are expressed per role in the skin, which is the only way a
  // per-skin font and a global size can coexist without one silently
  // overriding the other. score::uiFontSize() is only the size the
  // application font has before any skin has loaded.
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::Skin& skin = score::Skin::instance();
    QSettings s;

    const QJsonObject withFonts
        = read_skin(QStringLiteral(":/skin/GalmuriMicroSkin.json"));
    const QJsonObject dflt = read_skin(QStringLiteral(":/skin/DefaultSkin.json"));
    REQUIRE(withFonts.contains("fonts"));
    REQUIRE(dflt.contains("fonts"));

    // Skin/FontSize is not read by anything; writing it must change nothing.
    s.setValue("Skin/FontSize", 29);
    skin.load(dflt);
    const int fromSkin
        = dflt["fonts"].toObject()["application"].toObject()["pixelSize"].toInt();
    REQUIRE(fromSkin > 0);
    INFO("DefaultSkin asks for " << fromSkin << " px");
    CHECK(skin.ApplicationFont.pixelSize() == fromSkin);
    CHECK(skin.ApplicationFont.pixelSize() != 29);
    s.remove(QStringLiteral("Skin/FontSize"));

    // Switching skin switches the sizes, live.
    skin.load(withFonts);
    const int micro
        = withFonts["fonts"].toObject()["application"].toObject()["pixelSize"].toInt();
    CHECK(skin.ApplicationFont.pixelSize() == micro);
    CHECK(micro != fromSkin);

    skin.load(dflt);
    CHECK(skin.ApplicationFont.pixelSize() == fromSkin);
  });
}

TEST_CASE("The default skin keeps the sizes the point sizes resolved to",
          "[integration][skin]")
{
  // Sizes are in pixels so that the rasteriser is not left rounding through
  // the screen DPI. The values are the ones Qt resolves the equivalent point
  // sizes to at 96 DPI -- 12 pt -> 16 px, 9 pt -> 12 px, 13 pt -> 17 px -- so
  // that the UI is proportioned as it is on a 96 DPI screen.
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::Skin& skin = score::Skin::instance();
    skin.load(read_skin(QStringLiteral(":/skin/DefaultSkin.json")));

    CHECK(skin.SansFont.pixelSize() == 16);
    CHECK(skin.SansFontSmall.pixelSize() == 12);
    CHECK(skin.MonoFont.pixelSize() == 17);
    CHECK(skin.MonoFontSmall.pixelSize() == 12);

    // And nothing is left on a point size, which is the point of the change.
    for(auto& [role, f] : skin.fonts())
    {
      INFO("role '" << role << "' still uses a point size");
      CHECK(f->pixelSize() > 0);
    }
  });
}

TEST_CASE("Every built-in skin keeps its fonts on their pixel grid", "[integration][skin]")
{
  // The whole point of these fonts is that they are drawn on a grid, and they
  // only stay sharp at a whole multiple of it. A skin that asks for an
  // off-grid size still renders, just with uneven stem widths, so nothing
  // else would catch the mistake. The grids below were measured from the
  // font outlines.
  static const std::vector<std::pair<const char*, int>> grids{
      {"Galmuri7", 8},
      {"Galmuri9", 10},
      {"Galmuri11", 12},
      {"Galmuri14", 15},
      {"GalmuriMono7", 8},
      {"GalmuriMono9", 10},
      {"GalmuriMono11", 12},
      {"Departure Mono", 11},
      {"Cozette", 13},
      {"CozetteVector", 13},
      {"Ark Pixel 10px Prop latin", 10},
      {"Ark Pixel 12px Prop latin", 12},
      {"Ark Pixel 16px Prop latin", 16}};

  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // Needed for the styleName check below; per-application font database.
    score::registerApplicationFonts();

    QDirIterator it{QStringLiteral(":/skin"), {QStringLiteral("*.json")}, QDir::Files};
    int checked = 0;
    while(it.hasNext())
    {
      it.next();
      const QString file = it.filePath();
      const QJsonObject fonts = read_skin(file)["fonts"].toObject();

      for(const QString& role : fonts.keys())
      {
        const QJsonObject spec = fonts[role].toObject();
        const QString family = spec["family"].toString();
        const int px = spec["pixelSize"].toInt();
        if(family.isEmpty() || px <= 0)
          continue; // "defaults", or a role that only sets flags

        const auto grid = std::find_if(
            grids.begin(), grids.end(),
            [&](const auto& g) { return family == QLatin1String(g.first); });
        if(grid == grids.end())
          continue; // not one of the pixel fonts; any size is fine

        INFO(
            file.toStdString() << " role '" << role.toStdString() << "': " << px
                               << " px is not a multiple of " << family.toStdString()
                               << "'s " << grid->second << " px grid");
        CHECK(px % grid->second == 0);
        ++checked;

        // Asking for bold on a family with no Bold face makes Qt synthesise
        // one by smearing the glyphs, which turns a pixel font's 1 px stems
        // into ragged 2 px ones. A skin may only set bold on a family that
        // actually ships the face.
        const bool hasBoldFace
            = QFontDatabase::styles(family).contains(QStringLiteral("Bold"));
        INFO(
            file.toStdString() << " role '" << role.toStdString() << "' asks bold="
                               << spec["bold"].toBool() << " on "
                               << family.toStdString() << ", which "
                               << (hasBoldFace ? "has" : "does NOT have")
                               << " a Bold face");
        if(!hasBoldFace)
          CHECK_FALSE(spec["bold"].toBool());

        // Silence is not the same as false. Skin::load() rebuilds every font
        // through setupFonts() before applying the file, and that leaves
        // weights on several roles -- 900 on mono, 700 on the bold ones, 600
        // on slider and timecode. A role that pins no weight inherits one and
        // Qt matches it to the nearest face it has, or fakes it. So every
        // role of a pixel font has to say what weight it wants.
        INFO(
            file.toStdString() << " role '" << role.toStdString()
                               << "' pins no weight, so it inherits one");
        CHECK((spec.contains("bold") || spec.contains("weight")));

        if(const QString style = spec["styleName"].toString(); !style.isEmpty())
        {
          INFO(
              file.toStdString() << " role '" << role.toStdString() << "' wants style '"
                                 << style.toStdString() << "' which "
                                 << family.toStdString() << " does not have");
          CHECK(QFontDatabase::styles(family).contains(style));
        }
      }
    }
    // Guard against the loop silently matching nothing.
    CHECK(checked > 50);
  });
}
