// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Automation/Color/GradientAutomView.hpp>
#include <QGraphicsSceneMoveEvent>
#include <QPainter>
#include <QColorDialog>

namespace Gradient
{
View::View(QGraphicsItem* parent): LayerView{parent}
{
  this->setFlags(QGraphicsItem::ItemIsFocusable |
                QGraphicsItem::ItemClipsToShape);
}

void View::setGradient(const View::gradient_colors& c) {
  m_colors = c;
  m_origColors = c;
  update();
}

void View::setDataWidth(double x)
{
  m_dataWidth = x;
  update();
}

const constexpr double side = 7;
static const QPainterPath& triangle{[]
{
    QPainterPath path;
    path.moveTo(0, 0);
    path.lineTo(side, 0);
    path.lineTo(side / 2., side / 1.5);
    path.lineTo(0, 0);
    return path;
}()};

void View::paint_impl(QPainter* p) const
{
  if(m_colors.size() < 2)
    return;
  QLinearGradient g{QPointF{0, 0}, QPointF{m_dataWidth, 0.}};
  for(auto col : m_colors)
  {
    g.setColorAt(col.first, col.second);
  }
  p->fillRect(boundingRect(), g);

  QPen pen(QColor::fromRgba(qRgba(200, 200, 200, 150)), 1);
  QBrush br(Qt::gray);
  p->setPen(pen);
  p->setBrush(br);
  for(auto col : m_colors)
  {
    p->save();
    p->translate(col.first * m_dataWidth - side / 2., 0);
    p->setCompositionMode(QPainter::CompositionMode_Exclusion);
    p->drawLine(QPointF{side / 2., 0}, QPointF{side / 2., height()});
    p->setCompositionMode(QPainter::CompositionMode_SourceOver);
    p->drawPath(triangle);
    p->restore();
  }
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_clicked = ossia::none;

    const auto pos = event->pos();
    for(auto& e : m_colors)
    {
      if(std::abs(e.first * m_dataWidth - pos.x()) < 2.)
      {

        if(event->button() == Qt::LeftButton)
        {
          if(pos.y() < (side / 1.5))
          {
            auto w = QColorDialog::getColor();
            if(w.isValid())
              setColor(e.first, w);
          }
          else
          {
            m_clicked = e.first;
          }
        }

        else if(event->button() == Qt::RightButton)
        {
          removePoint(e.first);
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
      auto np = std::max(0., event->pos().x()) / m_dataWidth;

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
      auto np = event->pos().x() / m_dataWidth;

      m_colors.insert(std::make_pair(np, col));
      movePoint(*m_clicked, np);
      update();
    }
  }

  m_clicked = ossia::none;
}

void View::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  doubleClicked(event->pos());
}
}
