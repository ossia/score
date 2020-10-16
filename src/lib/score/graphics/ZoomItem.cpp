#include "ZoomItem.hpp"

#include <score/graphics/GraphicWidgets.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::ZoomItem)
namespace score
{

ZoomItem::ZoomItem(QGraphicsItem* parent)
  : QGraphicsItem{parent}
{
  static const auto plus_pixmap = score::get_pixmap(":/icons/zoom/plus.png");
  static const auto minus_pixmap = score::get_pixmap(":/icons/zoom/minus.png");
  static const auto center_pixmap = score::get_pixmap(":/icons/zoom/center.png");
  auto zplus = new score::QGraphicsPixmapButton{plus_pixmap, plus_pixmap, this};
  auto zminus = new score::QGraphicsPixmapButton{minus_pixmap, minus_pixmap, this};
  zminus->setPos({0, 15});
  auto zcenter = new score::QGraphicsPixmapButton{center_pixmap, center_pixmap, this};
  zcenter->setPos({0, 30});

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
