#include "GraphicsBoxLayout.hpp"

namespace score
{

GraphicsHBoxLayout::~GraphicsHBoxLayout()
{

}

void GraphicsHBoxLayout::layout()
{
  double x = 0;
  const double y = 0 + m_padding;
  double max_h = 0.;
  for(auto item : this->childItems())
  {
    const auto r = item->boundingRect();
    max_h = std::max(max_h, r.height());
    const auto item_x = x + m_padding;
    item->setPos(item_x, y);
    x = item_x + r.width() + m_padding;
  }

  // Make them fit the height
  for(auto item : this->childItems())
  {
    if(auto it = qgraphicsitem_cast<score::GraphicsLayout*>(item))
    {
      QRectF cur = it->rect();
      cur.setHeight(max_h);
      it->setRect(cur);
    }
  }
}

void GraphicsHBoxLayout::centerContent()
{
  // Center things vertically
  double total_h = boundingRect().height();
  for(auto item : this->childItems())
  {
    auto h = item->boundingRect().height();
    double rem = (total_h - h) / 2.;
    item->setPos(item->pos().x(), rem);
  }
}

GraphicsVBoxLayout::~GraphicsVBoxLayout()
{

}

void GraphicsVBoxLayout::layout()
{
  const double x = 0 + m_padding;
  double y = 0;

  double max_w = 0.;
  for(auto item : this->childItems())
  {
    const auto r = item->boundingRect();
    max_w = std::max(max_w, r.width());

    const auto item_y = y + m_padding;
    item->setPos(x, item_y);
    y = item_y + r.height() + m_padding;
  }

  // Make them fit the width
  for(auto item : this->childItems())
  {
    if(auto it = qgraphicsitem_cast<score::GraphicsLayout*>(item))
    {
      QRectF cur = it->rect();
      cur.setWidth(max_w);
      it->setRect(cur);
    }
  }
}

void GraphicsVBoxLayout::centerContent()
{
  // Center things horizontally
  double total_w = boundingRect().width();
  for(auto item : this->childItems())
  {
    auto w = item->boundingRect().width();
    double rem = (total_w - w) / 2.;
    item->setPos(rem, item->pos().y());
  }
}
}
