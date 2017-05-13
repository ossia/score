
#include <QBrush>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPen>
#include <Scenario/Document/TimeRuler/AbstractTimeRuler.hpp>
#include <cmath>
#include <qnamespace.h>
#include <Process/Style/ScenarioStyle.hpp>
#include <QGraphicsView>
#include <QTextLayout>

#include "AbstractTimeRulerView.hpp"
#include <Process/TimeValue.hpp>
#include <iscore/model/Skin.hpp>

class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
AbstractTimeRulerView::AbstractTimeRulerView(QGraphicsView* v)
    : m_width{800}
    , m_graduationsSpacing{10}
    , m_graduationDelta{10}
    , m_graduationHeight{-10}
    , m_intervalsBetweenMark{1}
    , m_viewport{v->viewport()}
{
  setY(-25);

  m_layout.setFont(iscore::Skin::instance().MonoFont);
}

void AbstractTimeRulerView::paint(
    QPainter* p, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if (m_width > 0.)
  {
    auto& painter = *p;
    auto& style = ScenarioStyle::instance();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(style.TimeRulerLargePen);
    painter.drawLine(0, 0, m_width, 0);
    painter.setPen(style.TimeRulerSmallPen);
    painter.setBrush(style.TimeRuler.getColor());
    painter.drawPath(m_path);

    painter.setFont(iscore::Skin::instance().MonoFont);

    for (const Mark& mark : m_marks)
    {
      painter.drawGlyphRun({mark.pos + 6., m_textPosition}, mark.text);
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
  if(format != m_timeFormat)
  {
    m_timeFormat = std::move(format);
    m_stringCache.clear();

    for (Mark& mark : m_marks)
    {
      mark.text = getGlyphs(mark.time);
    }
  }
}

void AbstractTimeRulerView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
}

void AbstractTimeRulerView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* ev)
{
  emit rescale();
  ev->accept();
}

void AbstractTimeRulerView::createRulerPath()
{
  m_marks.clear();
  m_marks.reserve(16);

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

  while (t < m_width + 1)
  {
    uint32_t res = (i % m_intervalsBetweenMark);
    if (res == 0)
    {
      m_marks.emplace_back(Mark{t, time, getGlyphs(time)});
      m_path.addRect(t, 0., 1., m_graduationHeight * 3.);
    }

    t += m_graduationsSpacing;
    time = time.addMSecs(m_graduationDelta);
    i++;
  }
  update();
  m_viewport->update();
}

QGlyphRun AbstractTimeRulerView::getGlyphs(const QTime& t)
{
  auto it = ossia::find_if(m_stringCache, [&] (auto& v) { return v.first == t; });
  if(it != m_stringCache.end())
  {
    return it->second;
  }
  else
  {
    m_layout.setText(t.toString(m_timeFormat));
    m_layout.beginLayout();
    auto line = m_layout.createLine();
    m_layout.endLayout();

    QGlyphRun gr;

    auto glr = line.glyphRuns();
    if (!glr.isEmpty())
        gr = glr.first();

    m_stringCache.push_back(std::make_pair(t, gr));
    if (m_stringCache.size() > 16)
        m_stringCache.pop_front();

    m_layout.clearLayout();
    
    return gr;
  }
}

void AbstractTimeRulerView::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  emit drag(ev->lastScenePos(), ev->scenePos());
  ev->accept();
}

void AbstractTimeRulerView::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
}
}
