#include "Loudness.hpp"

#include <Process/Dataflow/Port.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <Ui/Monitor/LayerHelpers.hpp>
#include <Ui/Monitor/Paint.hpp>

#include <numbers>

namespace Ui::Loudness
{
Layer::Layer(
    const Process::ProcessModel& process, const Process::Context& doc,
    QGraphicsItem* parent)
    : Process::EffectLayerView{parent}
{
  using namespace Ui::Monitor;
  setAcceptedMouseButtons(Qt::LeftButton);
  showInlet(process, 0, doc, this, this);
  ModeSwitch::make(process, doc, this, this);
  m_mode = controlInlet(process, 1);
  m_standard = controlInlet(process, 2);
  m_custom = controlInlet(process, 3);
  m_scale = controlInlet(process, 4);
  m_relative = controlInlet(process, 5);
  repaintOn(this, m_mode, m_standard, m_custom, m_scale, m_relative);

  m_link = new Process::MonitorLink{
      process, *this,
      [this](Process::Monitor& m) {
    auto& mon = static_cast<LoudnessMonitor&>(m);
    Readings r;
    if(mon.readings.consume(r))
    {
      // Several readings may have gone by: repeat the last one for each
      const auto steps = std::clamp<int64_t>(int64_t(r.sequence - m_now.sequence), 1, 20);
      if(r.sequence < m_now.sequence)
        m_history_count = 0;
      for(int i = 0; i < steps; i++)
      {
        m_history[m_history_pos] = {float(r.momentary), float(r.short_term)};
        m_history_pos = (m_history_pos + 1) % int(m_history.size());
        m_history_count = std::min(m_history_count + 1, int(m_history.size()));
      }
      m_now = r;
      update();
    }
  },
      [this](Process::Monitor*) {
    m_now = {};
    m_history_count = 0;
    update();
  }, this};
}

Mode Layer::mode() const
{
  return Ui::Monitor::enumValue(m_mode, Mode::Numbers, Mode::Radar);
}

Target Layer::target() const
{
  return targetFor(
      Ui::Monitor::enumValue(m_standard, Standard::EbuR128, Standard::Custom),
      Ui::Monitor::floatValue(m_custom, -23.f));
}

bool Layer::relative() const
{
  return Ui::Monitor::boolValue(m_relative, false);
}

//! Displayed range in LUFS, per EBU Tech 3341: target -18 to +9, or -36 to +18
std::pair<double, double> Layer::range() const
{
  const double t = target().lufs;
  return Ui::Monitor::enumValue(m_scale, Scale::Ebu9, Scale::Ebu18) == Scale::Ebu9
             ? std::pair{t - 18., t + 9.}
             : std::pair{t - 36., t + 18.};
}

QString Layer::format(double lufs) const
{
  if(!std::isfinite(lufs) || lufs < -100)
    return QStringLiteral("-inf");
  if(relative())
  {
    const double lu = lufs - target().lufs;
    return QStringLiteral("%1%2 LU").arg(lu >= 0 ? "+" : "").arg(lu, 0, 'f', 1);
  }
  return QStringLiteral("%1 LUFS").arg(lufs, 0, 'f', 1);
}

QColor Layer::zone(double lufs) const
{
  const auto t = target();
  if(!std::isfinite(lufs))
    return Ui::Monitor::textColor();
  if(std::abs(lufs - t.lufs) <= t.tolerance)
    return QColor(76, 175, 80);
  return lufs > t.lufs ? QColor(235, 45, 40) : QColor(94, 170, 242);
}

//! Starts the measurement over, as the Reset inlet does.
QRectF Layer::resetRect() const
{
  return {boundingRect().right() - 48., 0., 38., 13.};
}

void Layer::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  // The reset button, or a double click on the histogram
  const bool twice = m_doubleClick(*ev);
  if(!resetRect().contains(ev->pos()) && !(twice && mode() == Mode::Histogram))
  {
    // Anywhere else belongs to the node: selecting, moving
    ev->ignore();
    return;
  }
  if(auto* mon = m_link->monitor_as<LoudnessMonitor>())
    mon->reset.store(true, std::memory_order_relaxed);
  m_now = {};
  m_history_count = 0;
  update();
  ev->accept();
}

void Layer::paint_impl(QPainter* p) const
{
  using namespace Ui::Monitor;
  p->save();
  p->fillRect(boundingRect(), background());
  drawLabel(*p, resetRect(), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("reset"));
  const QRectF r = boundingRect().adjusted(6, 18, -6, -6);
  switch(mode())
  {
    case Mode::Numbers:
      paintNumbers(p, r);
      break;
    case Mode::History:
      paintHistory(p, r);
      break;
    case Mode::Histogram:
      paintHistogram(p, r);
      break;
    case Mode::Radar:
      paintRadar(p, r);
      break;
  }
  p->restore();
}

void Layer::paintNumbers(QPainter* p, const QRectF& r) const
{
  using namespace Ui::Monitor;
  const auto t = target();
  struct Row
  {
    QString label;
    QString value;
    QColor color;
  };
  const Row rows[]{
      {"Integrated", format(m_now.integrated), zone(m_now.integrated)},
      {"Short-term", format(m_now.short_term), zone(m_now.short_term)},
      {"Momentary", format(m_now.momentary), zone(m_now.momentary)},
      {"Range", QStringLiteral("%1 LU").arg(m_now.range, 0, 'f', 1), textColor()},
      {"True peak",
       m_now.true_peak > -100 ? QStringLiteral("%1 dBTP").arg(m_now.true_peak, 0, 'f', 1)
                              : QStringLiteral("-inf"),
       m_now.true_peak > t.max_true_peak ? overColor() : textColor()},
      {"PLR",
       std::isfinite(m_now.integrated) && m_now.true_peak > -100
           ? QStringLiteral("%1 LU").arg(m_now.true_peak - m_now.integrated, 0, 'f', 1)
           : QStringLiteral("-"),
       textColor()},
      {"Duration", QStringLiteral("%1 s").arg(m_now.duration, 0, 'f', 0), textColor()},
  };
  const int n = int(std::size(rows));
  // The integrated reading large, the others in a list
  const double big_h = std::min(r.height() * 0.35, 56.);
  QFont big = score::Skin::instance().Bold12Pt;
  big.setPixelSize(int(std::max(10., big_h * 0.6)));
  p->setFont(big);
  p->setPen(rows[0].color);
  p->drawText(QRectF(r.left(), r.top(), r.width(), big_h), Qt::AlignCenter, rows[0].value);
  drawLabel(
      *p, QRectF(r.left(), r.top() + big_h - 4, r.width(), 12), Qt::AlignCenter,
      QStringLiteral("integrated, target %1 LUFS").arg(t.lufs, 0, 'g', 3));

  const double y0 = r.top() + big_h + 12;
  const double lh = std::clamp((r.bottom() - y0) / (n - 1), 10., 22.);
  QFont f = smallFont();
  f.setPixelSize(int(std::clamp(lh * 0.6, 9., 14.)));
  p->setFont(f);
  for(int i = 1; i < n; i++)
  {
    const QRectF row{r.left(), y0 + (i - 1) * lh, r.width(), lh};
    if(row.bottom() > r.bottom() + 1)
      break;
    p->setPen(textColor());
    p->drawText(row, Qt::AlignLeft | Qt::AlignVCenter, rows[i].label);
    p->setPen(rows[i].color);
    p->drawText(row, Qt::AlignRight | Qt::AlignVCenter, rows[i].value);
  }
  if(!m_now.running)
    drawReadout(*p, r.topLeft(), {"paused"});
}

void Layer::paintHistory(QPainter* p, const QRectF& area) const
{
  using namespace Ui::Monitor;
  const QRectF r = area.adjusted(30, 0, 0, -12);
  if(r.width() < 10 || r.height() < 10)
    return;
  const auto [lo, hi] = range();
  const auto t = target();
  const bool rel = relative();
  auto y_of = [&](double v) {
    return r.bottom() - std::clamp((v - lo) / (hi - lo), 0., 1.) * r.height();
  };
  // Target band
  p->fillRect(
      QRectF(
          r.left(), y_of(t.lufs + t.tolerance), r.width(),
          y_of(t.lufs - t.tolerance) - y_of(t.lufs + t.tolerance)),
      QColor(76, 175, 80, 40));
  auto levels = linearTicks(lo, hi, r.height(), 18.);
  for(auto& tk : levels)
  {
    const double y = y_of(tk.value);
    p->setPen(tk.value == t.lufs ? QColor(76, 175, 80, 120) : gridColor());
    p->drawLine(QPointF{r.left(), y}, QPointF{r.right(), y});
    tk.label = QString::number(rel ? tk.value - t.lufs : tk.value, 'g', 3);
  }
  drawYAxisLabels(*p, r, levels, y_of);
  // One reading per pixel, the newest on the right; ten readings a second
  const int cols = int(r.width());
  const int shown = std::min(cols, m_history_count);
  const int n = int(m_history.size());
  p->setRenderHint(QPainter::Antialiasing, true);
  for(int curve = 0; curve < 2; curve++)
  {
    QPolygonF line;
    line.reserve(shown);
    for(int i = 0; i < shown; i++)
    {
      const auto& pt = m_history[(m_history_pos - shown + i + 2 * n) % n];
      const double v = curve == 0 ? pt.momentary : pt.short_term;
      line << QPointF{r.right() - shown + i, y_of(std::isfinite(v) ? v : lo)};
    }
    p->setPen(QPen(
        curve == 0 ? QColor(94, 200, 242, 140) : QColor(240, 200, 40), curve == 0 ? 1. : 1.6));
    p->drawPolyline(line);
  }
  if(std::isfinite(m_now.integrated))
  {
    p->setPen(QPen(QColor(235, 235, 235), 1, Qt::DashLine));
    p->drawLine(
        QPointF{r.left(), y_of(m_now.integrated)}, QPointF{r.right(), y_of(m_now.integrated)});
  }
  // Time along the bottom
  auto times = linearTicks(0., cols / 10., r.width(), 50., " s");
  for(auto& tk : times)
    tk.label = tk.value == 0 ? QStringLiteral("now") : "-" + tk.label;
  drawXAxisLabels(*p, r, times, [&](double v) { return r.right() - v * 10.; });
  if(isUnderMouse())
    drawReadout(
      *p, QPointF{r.left() + 2, r.top() + 2},
      {"M " + format(m_now.momentary), "S " + format(m_now.short_term),
       "I " + format(m_now.integrated)});
}

void Layer::paintHistogram(QPainter* p, const QRectF& area) const
{
  using namespace Ui::Monitor;
  const QRectF r = area.adjusted(4, 0, -4, -12);
  const auto [lo, hi] = range();
  const auto t = target();
  uint32_t peak = 1;
  for(auto v : m_now.histogram)
    peak = std::max(peak, v);
  auto x_of = [&](double v) { return r.left() + (v - lo) / (hi - lo) * r.width(); };
  p->fillRect(
      QRectF(
          x_of(t.lufs - t.tolerance), r.top(),
          x_of(t.lufs + t.tolerance) - x_of(t.lufs - t.tolerance), r.height()),
      QColor(76, 175, 80, 40));
  for(int b = 0; b < histogram_bins; b++)
  {
    const double v = histogram_low + b * histogram_step;
    if(v < lo || v >= hi)
      continue;
    const double h = double(m_now.histogram[b]) / peak * r.height();
    p->fillRect(
        QRectF(x_of(v), r.bottom() - h, std::max(1., x_of(v + histogram_step) - x_of(v) - 1), h),
        zone(v + histogram_step / 2));
  }
  const bool rel = relative();
  auto levels = linearTicks(lo, hi, r.width(), 40.);
  for(auto& tk : levels)
    tk.label = QString::number(rel ? tk.value - t.lufs : tk.value, 'g', 3);
  drawXAxisLabels(*p, r, levels, x_of);
  if(std::isfinite(m_now.integrated))
  {
    p->setPen(QPen(QColor(235, 235, 235), 1, Qt::DashLine));
    p->drawLine(
        QPointF{x_of(m_now.integrated), r.top()}, QPointF{x_of(m_now.integrated), r.bottom()});
  }
}

void Layer::paintRadar(QPainter* p, const QRectF& area) const
{
  using namespace Ui::Monitor;
  const double side = std::min(area.width(), area.height());
  const QPointF c = area.center();
  const double R = side / 2. - 10.;
  if(R < 10)
    return;
  const auto [lo, hi] = range();
  const auto t = target();
  auto r_of = [&](double v) { return std::clamp((v - lo) / (hi - lo), 0., 1.) * R; };
  p->setRenderHint(QPainter::Antialiasing, true);

  // One revolution: four minutes of short-term loudness, the newest at the
  // top, older readings going round clockwise. Consecutive readings in the
  // same zone and fading step are one fan, filled at once.
  static constexpr int revolution = 2400;
  static constexpr int fade_steps = 16;
  const int shown = std::min(revolution, m_history_count);
  const int n = int(m_history.size());
  auto point = [&](double rr, int i) {
    const double a = 2. * std::numbers::pi * i / revolution - std::numbers::pi / 2;
    return QPointF{c.x() + rr * std::cos(a), c.y() + rr * std::sin(a)};
  };
  p->setPen(Qt::NoPen);
  QPolygonF fan;
  QColor fan_color;
  auto flush = [&] {
    if(fan.size() > 2)
    {
      p->setBrush(fan_color);
      p->drawPolygon(fan);
    }
    fan.clear();
  };
  for(int i = 0; i < shown; i++)
  {
    const auto& pt = m_history[(m_history_pos - 1 - i + 2 * n) % n];
    if(!std::isfinite(pt.short_term))
    {
      flush();
      continue;
    }
    const int step = i * fade_steps / revolution;
    QColor col = zone(pt.short_term);
    col.setAlpha(int(200 - 150. * step / fade_steps));
    if(fan.empty() || col != fan_color)
    {
      flush();
      fan_color = col;
      fan << c;
    }
    const double rr = r_of(pt.short_term);
    fan << point(rr, i) << point(rr, i + 1);
  }
  flush();

  p->setBrush(Qt::NoBrush);
  for(double v = std::ceil(lo / 6.) * 6.; v <= hi; v += 6.)
  {
    p->setPen(v == t.lufs ? QPen(QColor(76, 175, 80), 1.5) : QPen(gridColor(), 1));
    p->drawEllipse(c, r_of(v), r_of(v));
  }
  p->setPen(QPen(QColor(76, 175, 80), 1.5));
  p->drawEllipse(c, r_of(t.lufs), r_of(t.lufs));

  // Momentary loudness as an outer ring
  if(std::isfinite(m_now.momentary))
  {
    const double span = std::clamp((m_now.momentary - lo) / (hi - lo), 0., 1.) * 360.;
    p->setPen(QPen(zone(m_now.momentary), 5, Qt::SolidLine, Qt::FlatCap));
    p->drawArc(
        QRectF(c.x() - R - 6, c.y() - R - 6, 2 * R + 12, 2 * R + 12), 90 * 16,
        int(-span * 16));
  }
  QFont big = score::Skin::instance().Bold12Pt;
  big.setPixelSize(int(std::clamp(R * 0.18, 9., 28.)));
  p->setFont(big);
  p->setPen(zone(m_now.integrated));
  p->drawText(
      QRectF(c.x() - R, c.y() - R * 0.2, 2 * R, R * 0.4), Qt::AlignCenter,
      format(m_now.integrated));
}
}
