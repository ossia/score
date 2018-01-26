// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <QBrush>
#include <QGraphicsSceneEvent>
#include <QPainter>
#include <QPen>
#include <Scenario/Document/TimeRuler/TimeRuler.hpp>
#include <cmath>
#include <qnamespace.h>
#include <Process/Style/ScenarioStyle.hpp>
#include <QGraphicsView>
#include <QTextLayout>

#include <fmt/format.h>
#include <Process/TimeValue.hpp>
#include <score/model/Skin.hpp>
#include <ossia/detail/algorithms.hpp>

class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{

static const constexpr std::array<std::pair<double, std::chrono::microseconds>, 22> graduations
{{
  {0.1, std::chrono::seconds(120)},
  {0.2, std::chrono::seconds(60)},
  {0.5, std::chrono::seconds(30)},

  {1, std::chrono::seconds(10)},
  {2, std::chrono::seconds(5)},
  {5, std::chrono::seconds(2)},

  {10, std::chrono::milliseconds(1000)},
  {20, std::chrono::milliseconds(500)},
  {40, std::chrono::milliseconds(250)},
  {80, std::chrono::milliseconds(150)},

  {100, std::chrono::milliseconds(100)},
  {200, std::chrono::milliseconds(50)},
  {500, std::chrono::milliseconds(20)},

  {1000, std::chrono::milliseconds(10)},
  {2000, std::chrono::milliseconds(5)},
  {5000, std::chrono::milliseconds(2)},

  {10000, std::chrono::microseconds(1000)},
  {20000, std::chrono::microseconds(500)},
  {50000, std::chrono::microseconds(200)},

  {100000, std::chrono::microseconds(100)},
  {200000, std::chrono::microseconds(50)},
  {500000, std::chrono::microseconds(20)}
}};


TimeRuler::TimeRuler(QGraphicsView* v)
    : m_width{800}
    , m_graduationsSpacing{10}
    , m_graduationDelta{10}
    , m_graduationHeight{-15}
    , m_intervalsBetweenMark{1}
    , m_viewport{v->viewport()}
{
  setY(-24);

  m_layout.setFont(score::Skin::instance().MonoFont);

  this->setCacheMode(QGraphicsItem::NoCache);
  m_height = -2 * m_graduationHeight;
  m_textPosition = 1.65 * m_graduationHeight;
  this->setX(10);
}

QRectF TimeRuler::boundingRect() const
{
  return QRectF{0, -m_height, m_width * 2, m_height};
}
void TimeRuler::paint(
    QPainter* p, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if (m_width > 0.)
  {
    auto& painter = *p;
    auto& style = ScenarioStyle::instance();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(style.TimeRulerLargePen);
    painter.drawLine(QPointF{0., 0.}, QPointF{m_width, 0.});
    painter.setPen(style.TimeRulerSmallPen);
    painter.setBrush(style.TimeRuler.getBrush());
    painter.drawPath(m_path);

    //painter.setFont(score::Skin::instance().MonoFont);

    for (const Mark& mark : m_marks)
    {
      painter.drawGlyphRun({mark.pos + 6., m_textPosition}, mark.text);
    }
  }
}

void TimeRuler::setHeight(qreal newHeight)
{
  prepareGeometryChange();
  m_height = newHeight;
}

void TimeRuler::setWidth(qreal newWidth)
{
  prepareGeometryChange();
  m_width = newWidth;
  createRulerPath();
}

void TimeRuler::setStartPoint(TimeVal dur)
{
  ossia::time_value v{(int64_t)dur.msec()};
  if (m_startPoint != v)
  {
    m_startPoint = v;
    computeGraduationSpacing();
  }
}

void TimeRuler::setPixelPerMillis(double factor)
{
  if (factor != m_pixelPerMillis)
  {
    m_pixelPerMillis = factor;
    computeGraduationSpacing();
  }
}

void TimeRuler::computeGraduationSpacing()
{
  double pixPerSec = 1000. * m_pixelPerMillis;
  m_graduationsSpacing = pixPerSec;

  m_graduationDelta = 100.;
  m_intervalsBetweenMark = 5;

  int i = 0;
  const constexpr int n = graduations.size();
  for (i = 0; i < n - 1; i++)
  {
    if (pixPerSec > graduations[i].first
        && pixPerSec < graduations[i + 1].first)
    {
      m_graduationDelta = graduations[i].second.count() / 1000.;
      m_graduationsSpacing = pixPerSec * graduations[i].second.count() / 1000000.;
      break;
    }
  }

  auto oldFormat = m_timeFormat;
  if (i > 7)
  {
    m_timeFormat = Format::Milliseconds;
    m_intervalsBetweenMark = 10;
  }
  else
  {
    m_timeFormat = Format::Seconds;
  }
  if(oldFormat != m_timeFormat)
    m_stringCache.clear();

  createRulerPath();
}

void TimeRuler::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
}

void TimeRuler::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* ev)
{
  emit rescale();
  ev->accept();
}

void TimeRuler::createRulerPath()
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
      = std::floor(m_startPoint.impl / big_delta) * big_delta;

  double startTime = m_startPoint.impl - prev_big_grad_msec;
  std::chrono::microseconds time{1000 * (int64_t)prev_big_grad_msec};
  double t = -startTime * m_pixelPerMillis;

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

QGlyphRun TimeRuler::getGlyphs(std::chrono::microseconds t)
{
  auto it = ossia::find_if(m_stringCache, [&] (auto& v) { return v.first == t; });
  if(it != m_stringCache.end())
  {
    return it->second;
  }
  else
  {
    if(m_timeFormat == Format::Seconds)
    {
      auto clean_duration = break_down_durations<std::chrono::minutes, std::chrono::seconds>(t);
      m_layout.setText(QString::fromStdString( fmt::format("{0}:{1:02}", std::get<0>(clean_duration).count(), std::get<1>(clean_duration).count())));
    }
    else if(m_timeFormat == Format::Milliseconds)
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
        gr = std::move(glr.first());

    m_stringCache.push_back(std::make_pair(t, gr));
    if (m_stringCache.size() > 16)
        m_stringCache.pop_front();

    m_layout.clearLayout();

    return gr;
  }
}

void TimeRuler::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  emit drag(ev->lastScenePos(), ev->scenePos());
  ev->accept();
}

void TimeRuler::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
}
}
