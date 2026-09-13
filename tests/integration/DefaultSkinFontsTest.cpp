// DefaultSkin's "fonts" block sets the default fonts; Skin::setupFonts() is
// the fallback for skins naming nothing. The values are deliberately not
// pinned here -- editing them is the point of the file -- only that the block
// is wired up and cannot fail silently.

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

      // loadFonts() ignores an unknown key, so a typo never applies.
      INFO("'" << key.toStdString() << "' is not one of the Skin's font roles");
      CHECK(known.contains(key));

      const QString family = fonts[key].toObject()["family"].toString();
      if(!family.isEmpty())
      {
        // An unshipped family resolves to something else with no warning.
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
