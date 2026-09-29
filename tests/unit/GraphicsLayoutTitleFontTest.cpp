// The section and strip cell titles follow the skin's font when it changes
// (skin editor, skin load) after the title was set.

#include <score/graphics/layouts/GraphicsBoxLayout.hpp>
#include <score/graphics/layouts/GraphicsStripDetailLayout.hpp>
#include <score/model/Skin.hpp>

#include <score_test/App.hpp>

#include <QImage>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <catch2/catch_test_macros.hpp>

namespace
{
QImage render(QGraphicsItem& item)
{
  QImage img{120, 40, QImage::Format_ARGB32_Premultiplied};
  img.fill(Qt::transparent);
  QPainter p{&img};
  QStyleOptionGraphicsItem opt;
  item.paint(&p, &opt, nullptr);
  return img;
}

struct RestoreFont
{
  QFont& font;
  QFont saved{font};
  ~RestoreFont() { font = saved; }
};
}

TEST_CASE("layout titles follow a skin font change", "[layout]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    auto& skin = score::Skin::instance();
    RestoreFont restore{skin.Medium8Pt};
    const QString title = QStringLiteral("A rather long section title");

    score::GraphicsSectionLayout section{nullptr};
    section.setTitle(title);
    section.layout();
    const double before = section.rect().width();

    score::GraphicsStripCell cell{nullptr};
    cell.setTitle(title);
    cell.setRect({0., 0., 100., 30.});
    render(cell);

    QFont bigger = skin.Medium8Pt;
    bigger.setPixelSize(2 * QFontInfo{skin.Medium8Pt}.pixelSize());
    skin.Medium8Pt = bigger;

    section.layout();
    CHECK(section.rect().width() > before);

    // Painted as a cell made after the change
    score::GraphicsStripCell fresh{nullptr};
    fresh.setTitle(title);
    fresh.setRect({0., 0., 100., 30.});
    CHECK(render(cell) == render(fresh));
  });
}
