// DefaultSkin's "fonts" block is where the default fonts are set. The C++ in
// Skin::setupFonts() is the fallback, for skins that name nothing and for the
// window that exists before any skin has loaded.
//
// So this deliberately does NOT pin the values: changing a default font is a
// matter of editing the skin, and a test asserting the block still equals the
// C++ would stand in the way of exactly that. What it checks is that the block
// is wired up and cannot fail silently:
//
//   * every key names a real role, so a typo is caught rather than ignored;
//   * every family named is actually available, since a missing one falls
//     back to some other font with no error;
//   * the block genuinely reaches the Skin, rather than being parsed and
//     dropped.

#include <score_test/App.hpp>

#include <score/model/Skin.hpp>

#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <catch2/catch_test_macros.hpp>

namespace
{
QJsonObject default_skin()
{
  QFile f(QStringLiteral(":/skin/DefaultSkin.json"));
  REQUIRE(f.open(QFile::ReadOnly));
  return QJsonDocument::fromJson(f.readAll()).object();
}
}

TEST_CASE("DefaultSkin's font block is wired up", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::registerApplicationFonts();

    const QJsonObject doc = default_skin();
    REQUIRE(doc.contains("fonts"));
    const QJsonObject fonts = doc["fonts"].toObject();

    QSet<QString> known;
    for(auto& [key, f] : score::Skin::instance().fonts())
      known.insert(QString::fromUtf8(key));

    int named = 0;
    for(const QString& key : fonts.keys())
    {
      if(key.startsWith('_') || key == "defaults")
        continue;

      // A key that is not a role is silently ignored by loadFonts(), so a
      // typo would simply never apply.
      INFO("'" << key.toStdString() << "' is not one of the Skin's font roles");
      CHECK(known.contains(key));

      const QString family = fonts[key].toObject()["family"].toString();
      if(!family.isEmpty())
      {
        // A family score does not ship resolves to something else with no
        // warning, which is how you end up with one smooth label in a
        // pixel-font UI.
        INFO("role '" << key.toStdString() << "' names the family '"
                      << family.toStdString() << "', which is not available");
        CHECK(QFontDatabase::families().contains(family));
      }
      ++named;
    }
    CHECK(named > 0);
  });
}

TEST_CASE("DefaultSkin's fonts actually reach the Skin", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::registerApplicationFonts();
    score::Skin& skin = score::Skin::instance();

    const QJsonObject fonts = default_skin()["fonts"].toObject();
    skin.load(default_skin());

    for(auto& [key, f] : skin.fonts())
    {
      const QJsonObject spec = fonts[QLatin1String(key)].toObject();
      if(spec.isEmpty())
        continue;

      if(const QString family = spec["family"].toString(); !family.isEmpty())
      {
        INFO("role '" << key << "' should have taken family " << family.toStdString());
        CHECK(f->families().value(0) == family);
      }
      if(const int px = spec["pixelSize"].toInt(); px > 0)
      {
        INFO("role '" << key << "' should have taken pixelSize " << px);
        CHECK(f->pixelSize() == px);
      }
      if(const int pt = spec["pointSize"].toInt(); pt > 0 && spec["pixelSize"].toInt() <= 0)
      {
        INFO("role '" << key << "' should have taken pointSize " << pt);
        CHECK(f->pointSize() == pt);
      }
    }
  });
}
