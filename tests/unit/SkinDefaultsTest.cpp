// What a skin file that names only part of the colours leaves behind, and
// what a run with the colours switched off starts from.

#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/Skin.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <QJsonArray>
#include <QJsonObject>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace
{
struct TestApplication final : public score::ApplicationInterface
{
  score::ApplicationSettings appSettings;
  score::ApplicationComponentsData compData;
  score::ApplicationComponents comps{compData};
  score::DocumentList docList;
  std::vector<std::unique_ptr<score::SettingsDelegateModel>> settingsVec;
  score::ApplicationContext ctx{appSettings, comps, docList, settingsVec};

  TestApplication()
  {
    appSettings.gui = false; // Skin::instance() -> NoGUI skin (no QFont use)
    m_instance = this;
  }

  const score::ApplicationContext& context() const override { return ctx; }
  const score::ApplicationComponents& components() const override { return comps; }
};
static TestApplication g_app;

QJsonArray rgb(int r, int g, int b)
{
  return QJsonArray{r, g, b};
}
}

TEST_CASE("every skin colour is reachable by name", "[skin]")
{
  auto& skin = score::Skin::instance();

  const auto colours = skin.getColors();
  CHECK(colours.size() == 50);
  for(const auto& [colour, name] : colours)
  {
    INFO(name.toStdString());
    CHECK(skin.fromString(name) != nullptr);
  }
}

TEST_CASE("a skin names only what it changes", "[skin]")
{
  auto& skin = score::Skin::instance();

  QJsonObject obj;
  obj["Base1"] = rgb(1, 2, 3);
  skin.load(obj, score::Skin::Colours);

  CHECK(skin.Base1.color() == QColor(1, 2, 3));

  // Everything the file left out is the built-in colour, not black.
  CHECK(skin.Base2.color() == QColor(3, 150, 250));
  CHECK(skin.Base5.color() == QColor(24, 24, 24));
  CHECK(skin.Emphasis1.color() == QColor(0, 255, 255));
  CHECK(skin.Background1.color() == QColor(31, 31, 32));
  CHECK(skin.Cable1.color() == QColor(153, 102, 102, 136));
  CHECK(skin.Waveform1.color() == QColor(250, 180, 15));

  for(const auto& [colour, name] : skin.getColors())
  {
    if(name == "Dark")
      continue;
    INFO(name.toStdString());
    CHECK(colour != QColor(Qt::black));
  }
}

TEST_CASE("a skin does not inherit the previous one's colours", "[skin]")
{
  auto& skin = score::Skin::instance();

  QJsonObject first;
  first["Base2"] = rgb(10, 20, 30);
  skin.load(first, score::Skin::Colours);
  REQUIRE(skin.Base2.color() == QColor(10, 20, 30));

  skin.load(QJsonObject{}, score::Skin::Colours);
  CHECK(skin.Base2.color() == QColor(3, 150, 250));
}

TEST_CASE("Transparent1 keeps its hand-made shades", "[skin]")
{
  auto& skin = score::Skin::instance();
  skin.load(QJsonObject{}, score::Skin::Colours);

  const QColor base{19, 19, 19};
  CHECK(skin.Transparent1.color() == base);
  CHECK(skin.Transparent1.darker.brush.color() == base.lighter(180));
  CHECK(skin.Transparent1.darker300.brush.color() == base.lighter());
  CHECK(skin.Transparent1.lighter.brush.color() == skin.Gray.color());
  CHECK(skin.Transparent1.lighter180.brush.color() == skin.HalfLight.color());
}

TEST_CASE("colours switched off leaves the built-in ones", "[skin]")
{
  auto& skin = score::Skin::instance();

  QJsonObject obj;
  obj["Base2"] = rgb(10, 20, 30);
  skin.load(obj, 0);

  CHECK(skin.Base2.color() == QColor(3, 150, 250));
  CHECK(skin.Transparent1.color() == QColor(19, 19, 19));
  CHECK(skin.Transparent1.darker300.brush.color() == QColor(19, 19, 19).lighter());
  CHECK(skin.Transparent1.lighter.brush.color() == QColor(127, 127, 127));
}

TEST_CASE("a saved skin round-trips", "[skin]")
{
  auto& skin = score::Skin::instance();
  skin.load(QJsonObject{}, score::Skin::Colours);

  const auto saved = skin.toJson();
  const auto before = skin.getColors();

  QJsonObject other;
  other["Base2"] = rgb(10, 20, 30);
  skin.load(other, score::Skin::Colours);
  skin.load(saved, score::Skin::Colours);

  CHECK(skin.getColors() == before);
}
