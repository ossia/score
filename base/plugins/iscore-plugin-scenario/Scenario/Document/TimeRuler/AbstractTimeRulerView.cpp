#include <Process/Style/ProcessFonts.hpp>
#include <QBrush>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPen>
#include <Scenario/Document/TimeRuler/AbstractTimeRuler.hpp>
#include <cmath>
#include <qnamespace.h>
#include <Process/Style/ScenarioStyle.hpp>
#include <QGraphicsView>

#include "AbstractTimeRulerView.hpp"
#include <Process/TimeValue.hpp>
#include <iscore/model/Skin.hpp>


class QWidget;

namespace Scenario
{
AbstractTimeRulerView::AbstractTimeRulerView(QWidget* viewport)
    : m_width{800}
    , m_graduationsSpacing{10}
    , m_graduationDelta{10}
    , m_graduationHeight{-10}
    , m_intervalsBetweenMark{1}
    , m_viewport{viewport}
{
  setAcceptedMouseButtons(Qt::AllButtons);
  setY(-25);
}

void AbstractTimeRulerView::paint(
    QPainter* p)
{
  if (m_width > 0.)
  {
    auto& painter = *p;
    auto& style = ScenarioStyle::instance();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(style.TimeRulerLargePen);
    painter.drawLine(0, 0, m_width, 0);
    painter.setPen(style.TimeRulerSmallPen);
    painter.drawPath(m_path);

    painter.setFont(iscore::Skin::instance().MonoFont);

    for (const Mark& mark : m_marks)
    {
      painter.drawStaticText(mark.pos + 6., m_textPosition, mark.text);
    }
  }
}

void AbstractTimeRulerView::setHeight(qreal newHeight)
{
  //prepareGeometryChange();
  m_height = newHeight;
  update();
}

void AbstractTimeRulerView::setWidth(qreal newWidth)
{
  //prepareGeometryChange();
  m_width = newWidth;
  createRulerPath();
  update();
}

void AbstractTimeRulerView::setGraduationsStyle(
    double size, int delta, QString format, int mark)
{
  m_graduationsSpacing = size;
  m_graduationDelta = delta;
  setFormat(std::move(format));
  m_intervalsBetweenMark = mark;
  createRulerPath();
  update();
}

void AbstractTimeRulerView::setFormat(QString format)
{
  const auto& font = iscore::Skin::instance().MonoFont;
  m_timeFormat = std::move(format);
  for (Mark& mark : m_marks)
  {
    mark.text.setText(mark.time.toString(m_timeFormat));
    mark.text.prepare(QTransform{}, font);
  }
}

void AbstractTimeRulerView::mousePressEvent(QMouseEvent* ev)
{
  m_lastScenePos = mapToScene(ev->localPos());
  ev->accept();
}

void AbstractTimeRulerView::createRulerPath()
{
  m_marks.clear();

  m_path = QPainterPath{};

  if (m_width == 0)
  {
    update();
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

  const auto& font = iscore::Skin::instance().MonoFont;
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
      m_marks.back().text.prepare(QTransform{}, font);
      gradSize = 3;
      m_path.addRect(t, 0, 1, m_graduationHeight * gradSize);
    }

    t += m_graduationsSpacing;
    time = time.addMSecs(m_graduationDelta);
    i++;
  }
  update();
  m_viewport->update();
}

void AbstractTimeRulerView::mouseMoveEvent(QMouseEvent* ev)
{
  auto pos = mapToScene(ev->localPos());
  emit drag(m_lastScenePos, pos);
  m_lastScenePos = pos;
  ev->accept();
}

void AbstractTimeRulerView::mouseReleaseEvent(QMouseEvent* ev)
{
  ev->accept();
}
}
