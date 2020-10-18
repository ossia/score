#include "ZoomItem.hpp"

#include <score/graphics/GraphicWidgets.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::ZoomItem)
namespace score
{

ZoomItem::ZoomItem(QGraphicsItem* parent)
  : QGraphicsItem{parent}
{
  static const auto plus_pixmap_on = score::get_pixmap(":/icons/zoom/plus_on.png");
  static const auto plus_pixmap_off = score::get_pixmap(":/icons/zoom/plus_off.png");
  static const auto minus_pixmap_on = score::get_pixmap(":/icons/zoom/minus_on.png");
  static const auto minus_pixmap_off = score::get_pixmap(":/icons/zoom/minus_off.png");
  static const auto center_pixmap_on = score::get_pixmap(":/icons/zoom/center_on.png");
  static const auto center_pixmap_off = score::get_pixmap(":/icons/zoom/center_off.png");
  auto zplus = new score::QGraphicsPixmapButton{plus_pixmap_on, plus_pixmap_off, this};
  auto zminus = new score::QGraphicsPixmapButton{minus_pixmap_on, minus_pixmap_off, this};
  zminus->setPos({0, 18});
  auto zcenter = new score::QGraphicsPixmapButton{center_pixmap_on, center_pixmap_off, this};
  zcenter->setPos({0, 36});

  setFlag(ItemHasNoContents, true);

  connect(zplus, &score::QGraphicsPixmapButton::clicked,
          this, &ZoomItem::zoom);
  connect(zminus, &score::QGraphicsPixmapButton::clicked,
          this, &ZoomItem::dezoom);
  connect(zcenter, &score::QGraphicsPixmapButton::clicked,
          this, &ZoomItem::recenter);
}

ZoomItem::~ZoomItem()
{

}

QRectF ZoomItem::boundingRect() const
{
  return {0,0,15,3*15};
}

void ZoomItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
}

}
