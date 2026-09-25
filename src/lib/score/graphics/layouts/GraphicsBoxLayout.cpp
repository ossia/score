#include "GraphicsBoxLayout.hpp"

#include <score/model/Skin.hpp>

#include <QEvent>
#include <QFontMetricsF>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>

namespace score
{

GraphicsHBoxLayout::~GraphicsHBoxLayout() { }

void GraphicsHBoxLayout::layout()
{
  double x = 0;
  const double y = 0 + m_padding;

  auto items = this->childItems();
  updateChildrenRects(items);

  double max_h = 0.;
  bool first = true;
  for(auto item : std::as_const(items))
  {
    const auto r = item->boundingRect();
    max_h = std::max(max_h, r.height());
    const auto item_x = x + (first ? m_padding : spacing());
    item->setPos(item_x, y);
    x = item_x + r.width();
    first = false;
  }

  // Make them fit the height
  for(auto item : std::as_const(items))
  {
    if(auto it = dynamic_cast<score::GraphicsLayout*>(item))
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

GraphicsVBoxLayout::~GraphicsVBoxLayout() { }

void GraphicsVBoxLayout::layout()
{
  const double x = 0 + m_padding;
  double y = 0;

  auto items = this->childItems();
  updateChildrenRects(items);

  double max_w = 0.;
  bool first = true;
  for(auto item : std::as_const(items))
  {
    const auto r = item->boundingRect();
    max_w = std::max(max_w, r.width());

    const auto item_y = y + (first ? m_padding : spacing());
    item->setPos(x, item_y);
    y = item_y + r.height();
    first = false;
  }

  // Make them fit the width
  for(auto item : std::as_const(items))
  {
    if(auto it = dynamic_cast<score::GraphicsLayout*>(item))
    {
      QRectF cur = it->rect();
      cur.setWidth(max_w);
      it->setRect(cur);
    }
  }
}

void GraphicsVBoxLayout::centerContent()
{
  return;
  // Center things horizontally
  double total_w = boundingRect().width();
  for(auto item : this->childItems())
  {
    auto w = item->boundingRect().width();
    double rem = (total_w - w) / 2.;
    item->setPos(rem, item->pos().y());
  }
}

GraphicsSectionLayout::~GraphicsSectionLayout() { }

void GraphicsSectionLayout::setTitle(const QString& title)
{
  m_title = title;
}

QFont GraphicsSectionLayout::titleFont()
{
  QFont f = score::Skin::instance().Medium8Pt;
  f.setCapitalization(QFont::AllUppercase);
  return f;
}

qreal GraphicsSectionLayout::titleHeight() const
{
  return m_title.isEmpty() ? 0. : QFontMetricsF{titleFont()}.height() + 2.;
}

void GraphicsSectionLayout::layout()
{
  auto items = this->childItems();
  updateChildrenRects(items);

  const double x = m_padding;
  double y = m_padding + titleHeight();
  double max_w = 0.;
  bool first = true;
  for(auto item : std::as_const(items))
  {
    const auto r = item->boundingRect();
    max_w = std::max(max_w, r.width());
    if(!first)
      y += spacing();
    item->setPos(x, y);
    y += r.height();
    first = false;
  }

  // Make them fit the width, like a vbox
  for(auto item : std::as_const(items))
  {
    if(auto it = dynamic_cast<score::GraphicsLayout*>(item))
    {
      QRectF cur = it->rect();
      cur.setWidth(max_w);
      it->setRect(cur);
    }
  }

  const double title_w
      = m_title.isEmpty() ? 0. : QFontMetricsF{titleFont()}.horizontalAdvance(m_title);
  setRect({0., 0., std::max(max_w, title_w) + 2. * m_padding, y + m_padding});
}

void GraphicsSectionLayout::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  GraphicsLayout::paint(painter, option, widget);
  if(m_title.isEmpty())
    return;

  auto& skin = score::Skin::instance();
  const QFont f = titleFont();
  painter->setFont(f);
  painter->setPen(skin.LightGray.main.pen1);
  painter->drawText(QPointF{m_padding, m_padding + QFontMetricsF{f}.ascent()}, m_title);
}

GraphicsSelectableRow::GraphicsSelectableRow(QGraphicsItem* parent)
    : GraphicsHBoxLayout{parent}
{
  // Presses on the controls of the row select it too
  setFiltersChildEvents(true);
  setAcceptHoverEvents(true);
  // Layouts take no presses; this one must, for the table to see those on its
  // background. It still lets them through, so the node can be dragged.
  setAcceptedMouseButtons(Qt::LeftButton);
}

GraphicsSelectableRow::~GraphicsSelectableRow() { }

void GraphicsSelectableRow::activate()
{
  if(onActivated)
    onActivated();
}

void GraphicsSelectableRow::setSelected(bool selected)
{
  m_selected = selected;
  update();
}

bool GraphicsSelectableRow::sceneEventFilter(QGraphicsItem* watched, QEvent* event)
{
  if(event->type() == QEvent::GraphicsSceneMousePress)
    activate();
  // The control still gets the press
  return false;
}

void GraphicsSelectableRow::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  m_hovered = true;
  update();
  GraphicsHBoxLayout::hoverEnterEvent(event);
}

void GraphicsSelectableRow::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  m_hovered = false;
  update();
  GraphicsHBoxLayout::hoverLeaveEvent(event);
}

void GraphicsSelectableRow::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  GraphicsHBoxLayout::paint(painter, option, widget);
  if(!m_selected && !m_hovered)
    return;

  // An outline, not a fill: the controls of the row keep their contrast. All
  // of it stays inside the row's bounds, which are all Qt repaints on change.
  auto& skin = score::Skin::instance();
  const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
  painter->setRenderHint(QPainter::Antialiasing, true);
  QColor line = m_selected ? skin.Base4.main.brush.color() : skin.Gray.main.brush.color();
  line.setAlphaF(m_selected ? 0.55 : 0.35);
  painter->setPen(QPen{line, 1.});
  painter->setBrush(Qt::NoBrush);
  painter->drawRoundedRect(r, 3., 3.);
  if(m_selected)
    painter->fillRect(QRectF{r.x() + 1., r.y() + 3., 2., r.height() - 6.}, skin.Base4);
  painter->setRenderHint(QPainter::Antialiasing, false);
}
}
