#include "GraphicsSplitLayout.hpp"

#include <QPainter>
#include <QPen>

namespace score
{
static constexpr const double split_side_padding = 5.;
static constexpr const double split_top_padding = 15.;
GraphicsSplitLayout::~GraphicsSplitLayout()
{

}

void GraphicsSplitLayout::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  auto& style = score::Skin::instance();
  GraphicsLayout::paint(painter, option, widget);

  const auto& items = this->childItems();
  const int N = int(items.size()) - 1;

  for(int i = 0; i < N; i++)
  {
    auto item = items[i];
    double x = item->pos().x() + item->boundingRect().width() + m_padding + split_side_padding;
    painter->setPen(style.DarkGray.main.pen2_solid_round_round);
    painter->drawLine(x, split_top_padding, x, this->rect().height() - split_top_padding);
  }
}

void GraphicsSplitLayout::layout()
{
  double x = 0;
  const double y = 0 + m_padding;
  double max_h = 0.;

  const auto items = this->childItems();
  for(int i = 0, N = items.size(); i < N; i++)
  {
    auto item = items[i];
    const auto r = item->boundingRect();
    max_h = std::max(max_h, r.height());
    const auto item_x = x + m_padding;
    item->setPos(item_x, y);
    x = item_x + r.width() + m_padding + 2 * split_side_padding;
  }

  // Make them fit the height
  for(auto item : items)
  {
    if(auto it = qgraphicsitem_cast<score::GraphicsLayout*>(item))
    {
      QRectF cur = it->rect();
      cur.setHeight(max_h);
      it->setRect(cur);
    }
  }
}
}
