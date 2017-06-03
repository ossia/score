#include <Automation/Color/GradientAutomView.hpp>
#include <QGraphicsSceneMoveEvent>
#include <QPainter>
#include <QColorDialog>
namespace Gradient
{

View::View(QGraphicsItem* parent): LayerView{parent}
{
  this->setFlag(QGraphicsItem::ItemIsFocusable, true);
}

void View::setGradient(const View::gradient_colors& c) {
  m_colors = c;
  m_origColors = c;
  update();
}

void View::paint_impl(QPainter* p) const
{
  if(m_colors.size() < 2)
    return;
  QLinearGradient g{QPointF{0, 0}, QPointF{width(), 0.}};
  for(auto col : m_colors)
  {
    g.setColorAt(col.first, col.second);
  }
  p->fillRect(boundingRect(), g);

  QPen pen(QColor::fromRgba(qRgba(200, 200, 200, 150)), 1);
  QBrush br(Qt::gray);
  const constexpr double side = 7;
  QPainterPath path;
  path.moveTo(0, 0);
  path.lineTo(side, 0);
  path.lineTo(side / 2., side / 1.5);
  path.lineTo(0, 0);
  p->setPen(pen);
  p->setBrush(br);
  for(auto col : m_colors)
  {
    p->save();
    p->translate(col.first * width() - side / 2., 0);
    p->setCompositionMode(QPainter::CompositionMode_Exclusion);
    p->drawLine(QPointF{side / 2., 0}, QPointF{side / 2., height()});
    p->setCompositionMode(QPainter::CompositionMode_SourceOver);
    p->drawPath(path);
    p->restore();
  }
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_clicked = ossia::none;

  const auto pos = event->pos();
  for(auto& e : m_colors)
  {
    if(std::abs(e.first * width() - pos.x()) < 2.)
    {
      if(pos.y() < 4)
      {
        auto w = QColorDialog::getColor();
        if(w.isValid())
          emit setColor(e.first, w);
      }
      else
      {
        m_clicked = e.first;
      }
      break;
    }
  }
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_clicked)
  {
    m_colors = m_origColors;
    auto cur_it = m_colors.find(*m_clicked);
    if(cur_it != m_colors.end())
    {
      auto col = cur_it->second;
      m_colors.erase(*m_clicked);
      auto np = event->pos().x() / width();

      m_colors.insert(std::make_pair(np, col));
      update();
    }
  }
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_clicked)
  {
    m_colors = m_origColors;
    auto cur_it = m_colors.find(*m_clicked);
    if(cur_it != m_colors.end())
    {
      auto col = cur_it->second;
      m_colors.erase(*m_clicked);
      auto np = event->pos().x() / width();

      m_colors.insert(std::make_pair(np, col));
      emit movePoint(*m_clicked, np);
      update();
    }
  }

  m_clicked = ossia::none;
}

void View::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  emit doubleClicked(event->pos());
}
}
