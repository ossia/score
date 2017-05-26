
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

#include <fmt/format.h>
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
    , m_graduationHeight{-15}
    , m_intervalsBetweenMark{1}
    , m_viewport{v->viewport()}
{
  setY(-24);

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
    painter.drawLine(QPointF{0, 0}, QPointF{m_width, 0});
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
    double size, double delta, QString format, double mark)
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
  double big_delta = m_graduationDelta * 5. * 2.;
  double prev_big_grad_msec
      = std::floor(m_pres->startPoint().msec() / big_delta) * big_delta;

  double startTime = m_pres->startPoint().msec() - prev_big_grad_msec;
  std::chrono::microseconds time{(int64_t)(1000. * prev_big_grad_msec)};
  double t = -startTime * m_pres->pixelsPerMillis();

  double i = 0;

  while (t < m_width + 1.)
  {
    double res = std::fmod(i, m_intervalsBetweenMark);
    if (res == 0)
    {
      m_marks.emplace_back(Mark{t, time, getGlyphs(time)});
      m_path.addRect(t, 0., 1., m_graduationHeight * 3.);
    }

    t += m_graduationsSpacing;
    time += std::chrono::microseconds((int64_t)(1000. * m_graduationDelta));
    i++;
  }
  update();
  m_viewport->update();
}

// Taken from https://stackoverflow.com/a/42139394/1495627
template<class...Durations, class DurationIn>
std::tuple<Durations...> break_down_durations( DurationIn d ) {
  std::tuple<Durations...> retval;
  using discard=int[];
  (void)discard{0,(void((
    (std::get<Durations>(retval) = std::chrono::duration_cast<Durations>(d)),
    (d -= std::chrono::duration_cast<DurationIn>(std::get<Durations>(retval)))
  )),0)...};
  return retval;
}

QGlyphRun AbstractTimeRulerView::getGlyphs(std::chrono::microseconds t)
{
  auto it = ossia::find_if(m_stringCache, [&] (auto& v) { return v.first == t; });
  if(it != m_stringCache.end())
  {
    return it->second;
  }
  else
  {
    if(m_timeFormat == "m:ss")
    {
      auto clean_duration = break_down_durations<std::chrono::minutes, std::chrono::seconds>(t);
      m_layout.setText(QString::fromStdString( fmt::format("{0}:{1:02}", std::get<0>(clean_duration).count(), std::get<1>(clean_duration).count())));
    }
    else if(m_timeFormat == "m:ss.z")
    {
      auto clean_duration = break_down_durations<std::chrono::minutes, std::chrono::seconds, std::chrono::milliseconds>(t);
      m_layout.setText(QString::fromStdString(fmt::format("{0}:{1:02}.{2:03}", std::get<0>(clean_duration).count(), std::get<1>(clean_duration).count(), std::get<2>(clean_duration).count())));
    }
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
