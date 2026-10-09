// Piano roll rows: tints taken from the skin, lighter for white keys and darker
// for black keys than the ground in every skin, and redrawn when the skin
// changes although the view rasterises them once.

#include <Midi/MidiStyle.hpp>
#include <Midi/MidiView.hpp>

#include <score/model/Skin.hpp>

#include <QFile>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>

#include <cstdlib>

namespace
{
QJsonObject readSkin(const QString& path)
{
  QFile f{path};
  REQUIRE(f.open(QFile::ReadOnly));
  return QJsonDocument::fromJson(f.readAll()).object();
}

QColor over(const QColor& tint, const QColor& ground)
{
  QImage img{1, 1, QImage::Format_ARGB32};
  img.fill(ground);
  QPainter p{&img};
  p.fillRect(QRect{0, 0, 1, 1}, tint);
  p.end();
  return img.pixelColor(0, 0);
}

bool colorsClose(const QColor& a, const QColor& b, int delta)
{
  return std::abs(a.red() - b.red()) <= delta
         && std::abs(a.green() - b.green()) <= delta
         && std::abs(a.blue() - b.blue()) <= delta
         && std::abs(a.alpha() - b.alpha()) <= delta;
}

QColor whiteRow()
{
  return Midi::MidiStyle::instance().whiteKeyBrush.color();
}
QColor blackRow()
{
  return Midi::MidiStyle::instance().blackKeyBrush.color();
}
}

// One application for the whole file: MidiStyle is a process-wide singleton
// whose skin connection lives as long as the QApplication it was made under.
TEST_CASE("Piano roll rows follow the skin", "[midi][skin]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto& skin = score::Skin::instance();

    // The default skin keeps its tints.
    {
      skin.load(readSkin(":/skin/DefaultSkin.json"));

      CHECK(colorsClose(whiteRow(), QColor(255, 255, 255, 14), 1));
      CHECK(colorsClose(blackRow(), QColor(0, 0, 0, 64), 1));

      // As rendered over the scene ground.
      const QColor ground = skin.Background1.color();
      CHECK(colorsClose(over(whiteRow(), ground), QColor(43, 43, 44), 1));
      CHECK(colorsClose(over(blackRow(), ground), QColor(23, 23, 24), 1));
    }

    // Another skin retints them.
    {
      skin.load(readSkin(":/skin/DraculaSkin.json"));
      CHECK(whiteRow() == QColor(248, 248, 242, 14));
      CHECK(blackRow() == QColor(25, 26, 33, 64));
    }

    // Light skins name their foreground Light and their ground Dark: the
    // white-key rows must still be the lighter ones.
    {
      skin.load(readSkin(":/skin/CatppuccinLatteSkin.json"));
      CHECK(whiteRow() == QColor(239, 241, 245, 14));
      CHECK(blackRow() == QColor(19, 20, 26, 64));
    }
    for(const char* name :
        {"DefaultSkin", "DraculaSkin", "NordSkin", "CatppuccinLatteSkin",
         "SolarizedLightSkin", "IEEESkin"})
    {
      INFO(name);
      skin.load(readSkin(QStringLiteral(":/skin/%1.json").arg(name)));
      const QColor ground = skin.Background1.color();
      CHECK(over(whiteRow(), ground).lightness() >= ground.lightness());
      CHECK(over(blackRow(), ground).lightness() < ground.lightness());
    }

    // The view rasterises its rows once: a skin change must redraw them.
    {
      skin.load(readSkin(":/skin/DefaultSkin.json"));

      QGraphicsScene scene;
      QGraphicsView view{&scene};
      view.resize(400, 200);
      auto* roll = new Midi::View{nullptr};
      scene.addItem(roll);
      roll->setWidth(300);
      roll->setRange(60, 71);
      // 12 rows of 10 px: C (60) spans y 109..118, C# (61) y 99..108.
      roll->setHeight(120);

      const QColor ground{31, 31, 32};
      const auto render = [&] {
        QImage img{300, 120, QImage::Format_ARGB32};
        img.fill(ground);
        QPainter p{&img};
        scene.render(&p, QRectF{0, 0, 300, 120}, QRectF{0, 0, 300, 120});
        p.end();
        return img;
      };

      const QImage before = render();
      CHECK(colorsClose(before.pixelColor(150, 104), over(QColor(0, 0, 0, 64), ground), 1));
      CHECK(colorsClose(
          before.pixelColor(150, 114), over(QColor(255, 255, 255, 14), ground), 1));

      skin.load(readSkin(":/skin/DraculaSkin.json"));
      const QImage after = render();
      CHECK(colorsClose(after.pixelColor(150, 104), over(QColor(25, 26, 33, 64), ground), 1));
      CHECK(colorsClose(
          after.pixelColor(150, 114), over(QColor(248, 248, 242, 14), ground), 1));

      delete roll;
    }

    skin.load(readSkin(":/skin/DefaultSkin.json"));
  });
}
