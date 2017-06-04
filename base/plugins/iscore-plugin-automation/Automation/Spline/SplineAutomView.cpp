#include <Automation/Spline/SplineAutomView.hpp>
#include <QGraphicsSceneMoveEvent>
#include <QPainter>
#include <QEasingCurve>
#include <QColorDialog>
#include <Process/Style/ScenarioStyle.hpp>
// Disclaimer:
// Part of the code comes from splineeditor.cpp from
// the Qt project:
// Copyright (C) 2016 The Qt Company Ltd.
// https://github.com/qt/qtdeclarative/blob/dev/tools/qmleasing/splineeditor.cpp

namespace Spline
{
View::View(QGraphicsItem* parent): LayerView{parent}
{
  this->setFlags(QGraphicsItem::ItemIsFocusable |
                 QGraphicsItem::ItemClipsToShape);
}

QPointF View::mapToCanvas(const QPointF &point) const
{
  return QPointF(point.x() * width(),
                 height() - point.y() * height());
}

QPointF View::mapFromCanvas(const QPointF& point) const
{
  return QPointF(point.x() / width() ,
                 1. - point.y() / height());
}

void View::paint_impl(QPainter* p) const
{
  auto& skin = ScenarioStyle::instance();
  auto& painter = *p;

  painter.setRenderHint(QPainter::Antialiasing);

  auto& segmt = skin.ConditionPen;
  segmt.setColor(qRgb(220, 170, 20));
  auto& dash = skin.TimenodePen;
  const auto N_segts = m_spline.segments();

  QPainterPath path;
  for (std::size_t i = 0U; i < N_segts; i++)
  {
    QPainterPath path;
    QPointF p0 = mapToCanvas(m_spline.getP0(i));
    QPointF p1 = mapToCanvas(m_spline.getP1(i));
    QPointF p2 = mapToCanvas(m_spline.getP2(i));
    QPointF p3 = mapToCanvas(m_spline.getP3(i));

    path.moveTo(p0);
    path.cubicTo(p1, p2, p3);

    painter.strokePath(path, segmt);

    painter.setPen(dash);
    painter.drawLine(p0, p1);
    painter.drawLine(p3, p2);
  }

  const auto pts = m_spline.points.size();
  for (std::size_t i = 0U; i < pts; ++i)
  {
    const auto& pt = mapToCanvas(m_spline.points.at(i));
    auto rp = m_spline.isRealPoint(i);
    const auto pointSize = rp ? 3 : 2.5;

    if (rp)
      painter.setBrush(QColor(170, 220, 20));
    else if (i == m_clicked)
      painter.setBrush(QColor(170, 220, 220));
    else
      painter.setBrush(QColor(170, 160, 160));

    painter.setPen(QPen(Qt::transparent));
    painter.drawEllipse(QRectF{pt.x() - pointSize,
                         pt.y() - pointSize,
                         pointSize * 2.,
                         pointSize * 2.
                        });
  }
}

void View::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
  auto btn = e->button();
  if (btn == Qt::LeftButton)
  {
    if ((m_clicked = findControlPoint(e->pos())))
    {
      mouseMoveEvent(e);
    }
    e->accept();
  }
  else if(btn == Qt::RightButton)
  {
    // Delete
  }
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
  QPointF p = mapFromCanvas(e->pos());
  if(!m_clicked)
    return;
  const auto mp = *m_clicked;
  const auto N = m_spline.points.size();
  if (mp < N)
  {
    // Move a real point
    if (m_spline.isRealPoint(mp))
    {
      QPointF targetPoint = p;
      QPointF distance = targetPoint - m_spline.points.at(mp);
      m_spline.points[mp] = targetPoint;
      if(mp == 0)
      {
        m_spline.points[mp + 1] += distance;
      }
      else if(mp == N - 1)
      {
        m_spline.points[mp - 1] += distance;
      }
      else
      {
        m_spline.points[mp - 1] += distance;
        m_spline.points[mp + 1] += distance;
      }
    }
    else
    {
      // Move a tangent
      m_spline.points[mp] = p;
    }

    update();
  }
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
  if (m_clicked)
  {
    emit changed();
    m_clicked = ossia::none;
  }
  e->accept();
}

void View::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  QPointF newPos = mapFromCanvas(event->pos());
  std::size_t splitIndex = 0;
  const std::size_t N = m_spline.points.size();
  for (std::size_t i = 0; i < N - 1; ++i)
  {
    auto real = m_spline.isRealPoint(i);
    if (real && m_spline.points[i].x() > newPos.x())
    {
      break;
    }
    else if (real)
    {
      splitIndex = i;
    }
  }
  QPointF before = m_spline.points.at(splitIndex);
  QPointF after = QPointF(1.0, 1.0);

  if ((splitIndex + 3) < N)
    after = m_spline.points.at(splitIndex + 3);

  m_spline.points.insert(m_spline.points.begin() + splitIndex + 2, (newPos + after) / 2.);
  m_spline.points.insert(m_spline.points.begin() + splitIndex + 2, newPos);
  m_spline.points.insert(m_spline.points.begin() + splitIndex + 2, (newPos + before) / 2.);

  emit changed();
  update();
}

optional<std::size_t> View::findControlPoint(QPointF point)
{
  int pointIndex = -1;
  qreal distance = -1;

  const auto N = m_spline.points.size();
  for (std::size_t i = 0; i < N; ++i)
  {
    qreal d = QLineF{point, mapToCanvas(m_spline.points.at(i))}.length();
    if ((distance < 0 && d < 10) || d < distance)
    {
      distance = d;
      pointIndex = i;
    }
  }

  if(pointIndex != -1)
    return pointIndex;
  return {};
}

}
