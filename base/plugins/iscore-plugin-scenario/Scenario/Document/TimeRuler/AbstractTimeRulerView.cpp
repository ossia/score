#include <Process/Style/ProcessFonts.hpp>
#include <QBrush>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPen>
#include <Scenario/Document/TimeRuler/AbstractTimeRuler.hpp>
#include <cmath>
#include <qnamespace.h>

#include "AbstractTimeRulerView.hpp"
#include <Process/TimeValue.hpp>
#include <iscore/model/Skin.hpp>


class QWidget;

namespace Scenario
{
AbstractTimeRulerView::AbstractTimeRulerView()
    : m_width{800}
    , m_graduationsSpacing{10}
    , m_graduationDelta{10}
    , m_intervalsBetweenMark{1}
    , m_graduationHeight{-10}
{
  setY(-25);
}

void AbstractTimeRulerView::paint(
    QPainter* p)
{
  auto& painter = *p;
  painter.setRenderHint(QPainter::Antialiasing, false);

  QPen pen{m_color.getBrush(), 2, Qt::SolidLine};
  painter.setPen(pen);
  painter.drawLine(0, 0, m_width, 0);

  pen.setWidth(1);
  painter.setPen(pen);
  painter.drawPath(m_path);

  painter.setFont(iscore::Skin::instance().MonoFont);

  if (m_width > 0)
  {
    for (const Mark& mark : m_marks)
    {
      painter.drawText(mark.pos + 6, m_textPosition, mark.text);
    }
  }
}

void AbstractTimeRulerView::setHeight(qreal newHeight)
{
  prepareGeometryChange();
  m_height = newHeight;
}

void AbstractTimeRulerView::setWidth(qreal newWidth)
{
  prepareGeometryChange();
  m_width = newWidth;
  createRulerPath();
}

void AbstractTimeRulerView::setGraduationsStyle(
    double size, int delta, QString format, int mark)
{
  prepareGeometryChange();
  m_graduationsSpacing = size;
  m_graduationDelta = delta;
  setFormat(std::move(format));
  m_intervalsBetweenMark = mark;
  createRulerPath();
}

void AbstractTimeRulerView::setFormat(QString format)
{
  m_timeFormat = std::move(format);
  for (Mark& mark : m_marks)
  {
    mark.text = mark.time.toString(m_timeFormat);
  }
}

void AbstractTimeRulerView::mousePressEvent(QMouseEvent* ev)
{
  ev->accept();
}

void AbstractTimeRulerView::createRulerPath()
{
  m_marks.clear();

  QPainterPath path;

  if (m_width == 0)
  {
    m_path = path;
    return;
  }

  // If we are between two graduations, we adjust our origin.
  double big_delta = m_graduationDelta * 5 * 2;
  double prev_big_grad_msec
      = std::floor(m_pres->startPoint().msec() / big_delta) * big_delta;

  TimeVal startTime
      = TimeVal::fromMsecs(m_pres->startPoint().msec() - prev_big_grad_msec);
  QTime time = TimeVal::fromMsecs(prev_big_grad_msec).toQTime();
  double t = -startTime.toPixels(1. / m_pres->pixelsPerMillis());

  uint32_t i = 0;
  double gradSize;

  while (t < m_width + 1)
  {
    /*
    gradSize = 0.5;
    if (m_intervalsBeetwenMark % 2 == 0)
    {
        if (i % (m_intervalsBeetwenMark / 2) == 0)
        {
            gradSize = 1;
        }
    }
    */

    uint32_t res = (i % m_intervalsBetweenMark);
    if (res == 0)
    {
      m_marks.push_back({t, time, time.toString(m_timeFormat)});
      gradSize = 3;
      path.addRect(t, 0, 1, m_graduationHeight * gradSize);
    }

    t += m_graduationsSpacing;
    time = time.addMSecs(m_graduationDelta);
    i++;
  }
  m_path = path;
  update();
}

void AbstractTimeRulerView::mouseMoveEvent(QMouseEvent* ev)
{
  emit drag(ev->lastScenePos(), ev->scenePos());
  ev->accept();
}

void AbstractTimeRulerView::mouseReleaseEvent(QMouseEvent* ev)
{
  ev->accept();
}
}
