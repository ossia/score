#include "GraphicsLayout.hpp"

#include <score/graphics/layouts/Constants.hpp>
#include <score/model/Skin.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QPainter>
namespace score
{

GraphicsLayout::GraphicsLayout(QGraphicsItem* parent)
    : score::BackgroundItem{parent}
    , m_bg{nullptr}
    , m_margin{default_margin}
    , m_padding{default_padding}
{
}

GraphicsLayout::~GraphicsLayout()
{
  delete m_pix;
}

void GraphicsLayout::layout()
{
  //fitChildrenRect();
}

void GraphicsLayout::centerContent() { }

void GraphicsLayout::setBrush(score::BrushSet& b)
{
  m_bg = &b;
}

void GraphicsLayout::setBackground(const QString& b)
{
  m_pix = new QPixmap{score::get_pixmap(":/plugins/" + b)};
}

void GraphicsLayout::setPadding(qreal p)
{
  m_padding = p;
}

void GraphicsLayout::setSpacing(qreal s)
{
  m_spacing = s;
}

qreal GraphicsLayout::spacing() const noexcept
{
  return m_spacing >= 0. ? m_spacing : 2. * m_padding;
}

void GraphicsLayout::updateChildrenRects(const QList<QGraphicsItem*>& items)
{
  for(int i = 0; i < items.size(); i++)
  {
    // An empty one has nothing to fit: it is a spacing item, whose size was
    // set on purpose and must not collapse to 0x0.
    if(auto rect = dynamic_cast<score::EmptyRectItem*>(items[i]);
       rect && !rect->childItems().isEmpty())
    {
      rect->fitChildrenRect();
    }
  }
}

void GraphicsLayout::setMargin(qreal p)
{
  m_margin = p;
}

void GraphicsLayout::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  // painter->fillRect(boundingRect(), QColor(qRgb(127, 127, 160)));
  if(m_bg)
  {
    auto& style = score::Skin::instance();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(style.NoPen);
    painter->setBrush(m_bg->brush);
    painter->drawRoundedRect(rect().adjusted(2., 2., -2., -2.), 3, 3);
    painter->setRenderHint(QPainter::Antialiasing, false);
  }
  else if(m_pix)
  {
    painter->drawPixmap(QPointF{}, *m_pix);
  }

  //painter->setBrush(Qt::transparent);
  //painter->setPen(Qt::red);
  //painter->drawRect(boundingRect());
  //painter->setPen(Qt::blue);
  //painter->drawRect(childrenBoundingRect());
}
}
