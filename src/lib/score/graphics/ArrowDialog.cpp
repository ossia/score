#include "ArrowDialog.hpp"
#include <QPainter>
#include <ossia/detail/algorithms.hpp>
namespace score
{

ArrowDialog::ArrowDialog(QGraphicsItem* parent)
  : QGraphicsItem{parent}
{

}

void ArrowDialog::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();
  painter->setPen(skin.Base5.lighter.pen1);
  painter->setBrush(skin.Base5.darker.brush);

  painter->drawPath(getPath(boundingRect().size()));
}

const QPainterPath& ArrowDialog::getPath(QSizeF sz)
{
  auto it = ossia::find_if(paths, [=] (const auto& pair) { return pair.first == sz; });
  if(it == paths.end())
  {
    return paths.emplace_back(sz, createPath(sz)).second;
  }
  else
  {
    return it->second;
  }
}

QPainterPath ArrowDialog::createPath(QSizeF sz)
{
  QPainterPath p;
  p.addRoundedRect(boundingRect(), 0, 3);
  p.moveTo({0, sz.height()});
  p.lineTo({0 + 6, sz.height() + 6});
  p.lineTo({0 + 2 * 6, sz.height()});
  //p.lineTo({3, sz.height()});
  p.closeSubpath();
  return p.simplified();
}

}
