// The interval name is rasterised into a pixmap by on_textChanged(), which
// only ever ran on name, label, hover and selection changes. A skin switch
// changes the font and the pen and moves none of those.

#include <score_test/App.hpp>

#include <score/model/Skin.hpp>

#include <Scenario/Document/Interval/IntervalHeader.hpp>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <catch2/catch_test_macros.hpp>

namespace
{
struct CountingHeader final : Scenario::IntervalHeader
{
  int redraws{};

  QRectF boundingRect() const override { return {}; }
  void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override { }
  void setState(State s) override { m_state = s; }
  void on_textChanged() override { redraws++; }
};
}

TEST_CASE("An interval header re-renders its name on a skin change", "[integration][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto read = [](const char* path) {
      QFile f{QString::fromUtf8(path)};
      REQUIRE(f.open(QFile::ReadOnly));
      return QJsonDocument::fromJson(f.readAll()).object();
    };

    auto& skin = score::Skin::instance();
    CountingHeader h;
    REQUIRE(h.redraws == 0);

    skin.load(read(":/skin/GalmuriMicroSkin.json"));
    CHECK(h.redraws == 1);

    skin.load(read(":/skin/DefaultSkin.json"));
    CHECK(h.redraws == 2);
  });
}
