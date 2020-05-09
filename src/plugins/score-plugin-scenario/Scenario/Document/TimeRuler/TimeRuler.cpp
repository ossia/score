// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Process/Style/ScenarioStyle.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/TimeRuler/TimeRuler.hpp>
#include <Scenario/Document/ScenarioDocument/MusicalGrid.hpp>

#include <score/graphics/YPos.hpp>
#include <score/model/Skin.hpp>
#include <score/graphics/PainterPath.hpp>

#include <ossia/detail/config.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QTextLayout>
#include <qnamespace.h>

#include <cmath>
#include <fmt/format.h>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::TimeRulerBase)
namespace Scenario
{

static const constexpr qreal graduationHeight = -15.;
static const constexpr qreal timeRulerHeight = 30.;
static const constexpr qreal textPosition = SCORE_YPOS(-22.75, -27.75);

static const constexpr std::
    array<std::pair<double, ossia::time_value>, 22>
        musical_graduations{{
                     {0.0,      ossia::time_value{32 * ossia::quarter_duration<int64_t>}},
                     {0.0125,   ossia::time_value{16 * ossia::quarter_duration<int64_t>}},
                     {0.025,    ossia::time_value{8 * ossia::quarter_duration<int64_t>}},
                     {0.05,     ossia::time_value{4 * ossia::quarter_duration<int64_t>}},
                     {0.1,      ossia::time_value{2 * ossia::quarter_duration<int64_t>}},
                     {0.2,      ossia::time_value{1 * ossia::quarter_duration<int64_t>}},
                     {0.5,      ossia::time_value{ossia::quarter_duration<int64_t> / 2}},

                     {1,        ossia::time_value{ossia::quarter_duration<int64_t> / 4}},
                     {2,        ossia::time_value{ossia::quarter_duration<int64_t> / 8}},
                     {5,        ossia::time_value{ossia::quarter_duration<int64_t> / 16}},

                     {10,       ossia::time_value{ossia::quarter_duration<int64_t> / 32}},
                     {20,       ossia::time_value{ossia::quarter_duration<int64_t> / 64}},
                     {40,       ossia::time_value{ossia::quarter_duration<int64_t> / 128}},
                     {80,       ossia::time_value{ossia::quarter_duration<int64_t> / 256}},

                     {100,      ossia::time_value{ossia::quarter_duration<int64_t> / 512}},
                     {200,      ossia::time_value{ossia::quarter_duration<int64_t> / 1024}},
                     {500,      ossia::time_value{ossia::quarter_duration<int64_t> / 2048}},

                     {1000,     ossia::time_value{ossia::quarter_duration<int64_t> / 4096}},
                     {2000,     ossia::time_value{ossia::quarter_duration<int64_t> / 8192}},
                     {5000,     ossia::time_value{ossia::quarter_duration<int64_t> / 16384}},

                     {10000,    ossia::time_value{ossia::quarter_duration<int64_t> / 32768}},
                     {20000,    ossia::time_value{ossia::quarter_duration<int64_t> / 65536}}
                    }
};


MusicalRuler::MusicalRuler(QGraphicsView* v)
    : m_viewport{v}
{
  m_width = 800;
  setY(-28.5);

  auto font = score::Skin::instance().MonoFont;
  font.setWeight(QFont::Normal);
  font.setPixelSize(11);
  font.setBold(false);
  m_layout.setFont(font);

  this->setCacheMode(QGraphicsItem::NoCache);
  this->setX(0);
}

void MusicalRuler::paint(
    QPainter* p,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  if (m_width > 0. && m_grid && !m_grid->mainPositions.empty())
  {
    auto& painter = *p;
    const auto& style = Process::Style::instance();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(style.TimeRulerSmallPen());
    painter.setBrush(style.TimeRuler());

    double last_pos = m_grid->mainPositions.front().pos_x;

    for(auto& [pos_x, timings, increment] : m_grid->mainPositions)
    {
      const auto& glyphs = getGlyphs(timings, increment);
      const auto w = glyphs.boundingRect().width();

      if(w < (pos_x - last_pos) || pos_x == last_pos)
      {
        painter.drawGlyphRun(QPointF{pos_x, textPosition}, glyphs);
        last_pos = pos_x;
      }
    }
  }
}


void MusicalRuler::setZoomRatio(double factor)
{
  update();
}

void MusicalRuler::computeGraduationSpacing()
{
  createRulerPath();
}

void MusicalRuler::createRulerPath()
{
  update();
  m_viewport->viewport()->update();
}


void layoutTimeText(ossia::bar_time timings, ossia::bar_time increment, QTextLayout& layout)
{
  QString txt;

  if(increment.bars > 0)
  {
    layout.setText(
          QString::fromStdString(
            fmt::format("{0}", timings.bars + 1)));
  }
  else if(increment.quarters > 0)
  {
    layout.setText(
          QString::fromStdString(
            fmt::format("{0}.{1}", timings.bars + 1, timings.quarters + 1)));
  }
  else if(increment.semiquavers > 0)
  {
    layout.setText(
          QString::fromStdString(
            fmt::format("{0}.{1}.{2}", timings.bars + 1, timings.quarters + 1, timings.semiquavers + 1)));
  }
  else if(increment.cents > 0)
  {
    layout.setText(
          QString::fromStdString(
            fmt::format("{0}.{1}.{2}.{3:03}", timings.bars + 1, timings.quarters + 1, timings.semiquavers + 1, timings.cents)));
  }
}



QGlyphRun MusicalRuler::getGlyphs(ossia::bar_time timings, ossia::bar_time increments)
{
  auto it
      = ossia::find_if(m_stringCache, [&](std::tuple<ossia::bar_time, ossia::bar_time, QGlyphRun>& v) {
      return std::get<0>(v) == timings && std::get<1>(v) == increments;
});
  if (it != m_stringCache.end())
  {
    return std::get<2>(*it);
  }
  else
  {
    layoutTimeText(timings, increments, m_layout);

    m_layout.beginLayout();
    auto line = m_layout.createLine();
    m_layout.endLayout();

    QGlyphRun gr;

    auto glr = line.glyphRuns();
    if (!glr.isEmpty())
      gr = std::move(glr.first());

    m_stringCache.push_back(std::make_tuple(timings, increments, gr));
    if (m_stringCache.size() > 16)
      m_stringCache.pop_front();

    m_layout.clearLayout();

    return gr;
  }
  return {};
}

void MusicalRuler::setGrid(MusicalGrid& grid)
{
  m_grid = &grid;
  connect(m_grid, &MusicalGrid::changed,
          this, [this] { update(); });
}







static const constexpr std::
    array<std::pair<double, std::chrono::nanoseconds>, 32>
        graduations{{
                     {0.0, std::chrono::seconds(1920)},
                     {0.0125, std::chrono::seconds(960)},
                     {0.025, std::chrono::seconds(480)},
                     {0.05, std::chrono::seconds(240)},
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
                     {500000, std::chrono::microseconds(20)},

                     {1000000, std::chrono::nanoseconds(1000)},
                     {2000000, std::chrono::nanoseconds(500)},
                     {5000000, std::chrono::nanoseconds(200)},

                     {10000000, std::chrono::nanoseconds(100)},
                     {20000000, std::chrono::nanoseconds(50)},
                     {50000000, std::chrono::nanoseconds(20)},
                    }
};





void TimeRulerBase::setWidth(qreal newWidth)
{
  prepareGeometryChange();
  m_width = newWidth;
  createRulerPath();
}

QRectF TimeRulerBase::boundingRect() const
{
  return QRectF{0, -timeRulerHeight, m_width * 2, timeRulerHeight};
}

void TimeRulerBase::setStartPoint(ossia::time_value dur)
{
  if (m_startPoint != dur)
  {
    m_startPoint = dur;
    computeGraduationSpacing();
  }
}

void TimeRulerBase::setGrid(MusicalGrid& grid)
{

}

void TimeRulerBase::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
}

void TimeRulerBase::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* ev)
{
  rescale();
  ev->accept();
}

void TimeRulerBase::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  drag(ev->lastScenePos(), ev->scenePos());
  ev->accept();
}

void TimeRulerBase::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
}



TimeRuler::TimeRuler(QGraphicsView* v)
    : m_graduationsSpacing{10}
    , m_graduationDelta{10}
    , m_intervalsBetweenMark{1}
    , m_viewport{v}
{
  m_width = 800;
  setY(-28.5);

  auto font = score::Skin::instance().MonoFont;
  font.setWeight(QFont::Normal);
  font.setPixelSize(11);
  font.setBold(false);
  m_layout.setFont(font);

  this->setCacheMode(QGraphicsItem::NoCache);
  this->setX(10);
}

void TimeRuler::paint(
    QPainter* p,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  if (m_width > 0.)
  {
    auto& painter = *p;
    const auto& style = Process::Style::instance();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(style.TimeRulerLargePen());
    painter.drawLine(QPointF{0., 0.}, QPointF{m_width, 0.});
    painter.setPen(style.TimeRulerSmallPen());
    painter.setBrush(style.TimeRuler());
    painter.drawPath(m_path);

    for (const Mark& mark : m_marks)
    {
      painter.drawGlyphRun({mark.pos + 6., textPosition}, mark.text);
    }
  }
}


void TimeRuler::setZoomRatio(double factor)
{
  factor = ossia::flicks_per_millisecond<double> / factor;
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
      m_graduationDelta = graduations[i].second.count() / double(1e6);
      m_graduationsSpacing = pixPerSec * graduations[i].second.count() / double(1e9);
      break;
    }
  }

  auto oldFormat = m_timeFormat;
  if (i > 17)
  {
    m_timeFormat = Format::Microseconds;
    m_intervalsBetweenMark = 10;
  }
  else if (i > 7)
  {
    m_timeFormat = Format::Milliseconds;
    m_intervalsBetweenMark = 10;
  }
  else if (i >= 1)
  {
    m_timeFormat = Format::Seconds;
  }
  else
  {
    m_timeFormat = Format::Hours;
  }

  if (oldFormat != m_timeFormat)
    m_stringCache.clear();

  createRulerPath();
}

void TimeRuler::createRulerPath()
{
  m_marks.clear();
  m_marks.reserve(16);

  clearPainterPath(m_path);

  if (m_width == 0)
  {
    update();
    return;
  }

  // If we are between two graduations, we adjust our origin.
  int64_t start_nsec = m_startPoint.impl / (ossia::flicks_per_second<double> / 1e9);

  double big_delta = m_graduationDelta * 5. * 2. * 1e6;
  double prev_big_grad_nsec
      = std::floor(start_nsec / big_delta) * big_delta;

  double startTime = start_nsec - prev_big_grad_nsec;
  std::chrono::nanoseconds time{(int64_t)(prev_big_grad_nsec)};
  double t = -startTime * (m_pixelPerMillis / 1e6);

  double i = 0;

  while (t < m_width + 1.)
  {
    double res = std::fmod(i, m_intervalsBetweenMark);
    if (res == 0)
    {
      m_marks.emplace_back(Mark{t, time, getGlyphs(time)});
      m_path.addRect(t, 0., 1., graduationHeight * 3.);
    }

    t += m_graduationsSpacing;
    time += std::chrono::nanoseconds((int64_t)(1000000. * m_graduationDelta));
    i++;
  }
  update();
  m_viewport->viewport()->update();
}

// Taken from https://stackoverflow.com/a/42139394/1495627
template <class... Durations, class DurationIn>
std::tuple<Durations...> break_down_durations(DurationIn d)
{
  std::tuple<Durations...> retval;
  using discard = int[];
  (void)discard{0,
                (void(
                     ((std::get<Durations>(retval)
                       = std::chrono::duration_cast<Durations>(d)),
                      (d -= std::chrono::duration_cast<DurationIn>(
                           std::get<Durations>(retval))))),
                 0)...};
  return retval;
}


void layoutTimeText(TimeRuler::Format format, QTextLayout& layout, std::chrono::nanoseconds t)
{

  switch(format)
  {
    case TimeRuler::Format::Hours:
    {
      auto clean_duration
          = break_down_durations<std::chrono::minutes>(
            t);
      layout.setText(QString::fromStdString(fmt::format(
                                              "{0}:00",
                                              std::get<0>(clean_duration).count())));
      break;
    }
    case TimeRuler::Format::Seconds:
    {
      auto clean_duration
          = break_down_durations<std::chrono::minutes, std::chrono::seconds>(
              t);
      layout.setText(QString::fromStdString(fmt::format(
          "{0}:{1:02}",
          std::get<0>(clean_duration).count(),
          std::get<1>(clean_duration).count())));
      break;
    }
    case TimeRuler::Format::Milliseconds:
    {
      auto [m, s, milli] = break_down_durations<
          std::chrono::minutes,
          std::chrono::seconds,
          std::chrono::milliseconds>(t);
      layout.setText(QString::fromStdString(fmt::format(
          "{0}:{1:02}.{2:03}",
          m.count(),
          s.count(),
          milli.count())));
    break;
    }
    case TimeRuler::Format::Microseconds:
    {
        auto [m, s, milli, micro] = break_down_durations<
          std::chrono::minutes,
          std::chrono::seconds,
          std::chrono::milliseconds,
          std::chrono::microseconds>(t);
      layout.setText(QString::fromStdString(fmt::format(
          "{0}:{1:02}.{2:03}{3:03}",
            m.count(),
            s.count(),
            milli.count(),
            micro.count())));
    break;
    }
    }
}



QGlyphRun TimeRuler::getGlyphs(std::chrono::nanoseconds t)
{
  auto it
      = ossia::find_if(m_stringCache, [&](auto& v) { return v.first == t; });
  if (it != m_stringCache.end())
  {
    return it->second;
  }
  else
  {
    layoutTimeText(m_timeFormat, m_layout, t);

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


}
