#include "GraphicsLayout.hpp"
#include <QPainter>

namespace score
{

GraphicsLayout::GraphicsLayout(QGraphicsItem* parent):
  score::BackgroundItem{parent}
, m_bg{nullptr}
{

}

GraphicsLayout::~GraphicsLayout()
{

}

void GraphicsLayout::layout()
{
  this->fitChildrenRect();
}

void GraphicsLayout::centerContent()
{

}

void GraphicsLayout::setBrush(score::BrushSet& b)
{
  m_bg = &b;
}

void GraphicsLayout::setPadding(qreal p)
{
  m_padding = p;
}

void GraphicsLayout::setMargin(qreal p)
{
  m_margin = p;
}

void GraphicsLayout::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  if(m_bg)
  {
    auto& style = score::Skin::instance();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(style.NoPen);
    painter->setBrush(m_bg->brush);
    painter->drawRoundedRect(rect().adjusted(2., 2., -2., -2.), 3, 3);
    painter->setRenderHint(QPainter::Antialiasing, false);
  }

  //painter->setBrush(Qt::transparent);
  //painter->setPen(Qt::red);
  //painter->drawRect(boundingRect());
  //painter->setPen(Qt::blue);
  //painter->drawRect(childrenBoundingRect());
}
}

