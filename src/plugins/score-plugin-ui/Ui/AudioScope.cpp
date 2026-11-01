#include "AudioScope.hpp"

#include <ossia/network/dataspace/gain.hpp>

#include <QGraphicsSceneMouseEvent>

#include <Process/Dataflow/Port.hpp>

#include <QPainter>
#include <QPainterPath>

#include <Ui/Monitor/LayerHelpers.hpp>

namespace Ui::AudioScope
{
Layer::Layer(
    const Process::ProcessModel& process, const Process::Context& doc,
    QGraphicsItem* parent)
    : Process::EffectLayerView{parent}
    , m_stack{doc.commandStack}
{
  using namespace Ui::Monitor;
  // For the double click; a press goes on to the node
  setAcceptedMouseButtons(Qt::LeftButton);
  showInlet(process, 0, doc, this, this);
  ModeSwitch::make(process, doc, this, this);
  m_mode = controlInlet(process, 1);
  m_window = controlInlet(process, 2);
  m_trigger = controlInlet(process, 3);
  m_level = controlInlet(process, 4);
  m_persistence = controlInlet(process, 5);
  m_gain = controlInlet(process, 6);
  m_auto = controlInlet(process, 7);
  m_x = controlInlet(process, 8);
  m_y = controlInlet(process, 9);
  m_polar = controlInlet(process, 10);
  repaintOn(this, m_mode, m_window, m_trigger, m_level, m_gain, m_auto, m_polar);

  m_link = new Process::MonitorLink{
      process, *this,
      [this](Process::Monitor& m) { refresh(static_cast<ScopeMonitor&>(m)); },
      [this](Process::Monitor*) { reset(); }, this};
}

Mode Layer::mode() const
{
  return Ui::Monitor::enumValue(m_mode, Mode::Oscilloscope, Mode::Correlation);
}
Trigger Layer::trigger() const
{
  return Ui::Monitor::enumValue(m_trigger, Trigger::Rising, Trigger::Pitch);
}
Polar Layer::polar() const
{
  return Ui::Monitor::enumValue(m_polar, Polar::Lissajous, Polar::PolarLevel);
}
double Layer::gain() const
{
  return ossia::detail::DecibelsToLinearGain(double(Ui::Monitor::floatValue(m_gain, 0.f)));
}
bool Layer::autoGain() const
{
  return Ui::Monitor::boolValue(m_auto, false);
}
float Layer::persistence() const
{
  return std::clamp(Ui::Monitor::floatValue(m_persistence, 0.f), 0.f, 0.98f);
}

void Layer::reset()
{
  m_trace.clear();
  m_phosphor.clear();
  m_dots.clear();
  m_corr_history.fill(0.f);
  m_corr_written = 0;
  m_channels = 0;
  m_read = 0;
  update();
}

void Layer::refresh(ScopeMonitor& mon)
{
  auto& ring = mon.ring;
  ring.provision();
  m_rate = ring.rate();
  m_channels = std::min(ring.channels(), max_shown_channels);
  m_corr = mon.correlation.load(std::memory_order_relaxed);
  refreshCorrelation(ring);
  if(m_channels == 0)
    return;

  switch(mode())
  {
    case Mode::Oscilloscope:
      refreshScope(ring);
      break;
    case Mode::XY:
    case Mode::Goniometer:
      refreshDots(ring);
      break;
    default:
      break;
  }
  update();
}

void Layer::refreshCorrelation(const Process::SampleRing& ring)
{
  // One entry per 1 / history_per_second of signal, whatever the refresh rate
  const auto hop = uint64_t(std::max(1., m_rate / history_per_second));
  const uint64_t w = ring.written();
  const int n = int(m_corr_history.size());
  if(m_corr_written == 0 || w < m_corr_written || w - m_corr_written > uint64_t(n) * hop)
    m_corr_written = w >= hop ? w - hop : 0;
  const auto count = int((w - m_corr_written) / hop);
  m_corr_written += uint64_t(count) * hop;
  for(int i = 0; i < std::min(count, n); i++)
  {
    m_corr_history[m_corr_pos] = m_corr;
    m_corr_pos = (m_corr_pos + 1) % n;
  }
}

void Layer::refreshScope(const Process::SampleRing& ring)
{
  const auto cap = int(ring.capacity());
  int window = int(std::clamp(
      Ui::Monitor::floatValue(m_window, 20.f) * m_rate / 1000., 8., cap / 4. - 1.));
  const auto trig = trigger();

  uint64_t first = 0;
  m_triggered = false;
  if(trig != Trigger::Free)
  {
    // Look for the trigger over the last two windows (more for pitch)
    const int span = trig == Trigger::Pitch ? std::min(cap / 4, 8192) : 2 * window;
    m_search.resize(span);
    uint64_t search_first{};
    if(ring.read_latest(0, m_search, search_first))
    {
      if(trig == Trigger::Pitch)
      {
        m_period = estimatePeriod(m_search.data(), span);
        if(m_period > 2)
          window = int(std::min(2. * m_period, span / 2.));
      }
      const bool rising = trig != Trigger::Falling;
      const float level
          = trig == Trigger::Pitch ? 0.f : Ui::Monitor::floatValue(m_level, 0.f);
      const int t = findTrigger(m_search.data(), span - window, level, rising);
      if(t >= 0)
      {
        first = search_first + t;
        m_triggered = true;
      }
    }
  }
  if(!m_triggered)
  {
    const uint64_t w = ring.written();
    if(w < uint64_t(window))
      return;
    first = w - window;
  }

  m_window_ms = window * 1000. / m_rate;
  m_trace.resize(m_channels);
  for(int c = 0; c < m_channels; c++)
  {
    m_trace[c].resize(window);
    if(!ring.read(c, first, m_trace[c]))
      std::fill(m_trace[c].begin(), m_trace[c].end(), 0.f);
  }

  const float pers = persistence();
  if(pers > 0.f)
  {
    const auto r = scopeRect();
    const double g = currentGain();
    m_phosphor.resize(m_channels);
    for(int c = 0; c < m_channels; c++)
    {
      auto& img = m_phosphor[c];
      img.resize(int(r.width()), int(r.height()));
      img.decay(pers);
      traceInto(img, m_trace[c], r.width(), r.height(), g);
      img.render(Ui::Monitor::channelColor(c), 2.f, 0.6f);
    }
  }
}

void Layer::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  if(!m_doubleClick(*ev))
  {
    ev->ignore();
    return;
  }
  ev->accept();
  auto* mon = m_link->monitor_as<ScopeMonitor>();
  if(!mon)
    return;
  // The highest sample of every channel over the last tenth of a second
  auto& ring = mon->ring;
  std::vector<float> buf(std::max(64, int(ring.rate() / 10.)));
  float peak = 0.f;
  uint64_t first{};
  for(int c = 0; c < ring.channels(); c++)
    if(ring.read_latest(c, buf, first))
      for(float v : buf)
        peak = std::max(peak, std::abs(v));
  if(peak <= 1e-6f)
    return;
  const double db = std::clamp(ossia::detail::LinearGainToDecibelsClipped(0.9 / peak), -24., 48.);
  Ui::Monitor::setControls(m_stack, {{m_gain, float(db)}, {m_auto, false}});
}

double Layer::currentGain() const
{
  if(!autoGain())
    return gain();
  float peak = 1e-4f;
  for(auto& t : m_trace)
    for(float v : t)
      peak = std::max(peak, std::abs(v));
  return 0.9 / peak;
}

void Layer::traceInto(
    Ui::Monitor::DensityImage& img, const std::vector<float>& x, double w, double h, double g)
{
  const int n = int(x.size());
  if(n < 2)
    return;
  const double mid = h / 2.;
  double px = 0, py = mid - x[0] * g * mid;
  for(int i = 1; i < n; i++)
  {
    const double nx = i * (w - 1) / (n - 1);
    const double ny = mid - x[i] * g * mid;
    img.line(px, py, nx, ny, 1.f);
    px = nx;
    py = ny;
  }
}

void Layer::refreshDots(const Process::SampleRing& ring)
{
  const int xc = std::clamp(int(Ui::Monitor::floatValue(m_x, 0)), 0, m_channels - 1);
  const int yc = std::clamp(int(Ui::Monitor::floatValue(m_y, 1)), 0, m_channels - 1);
  const uint64_t w = ring.written();
  const uint64_t max_n = 8192;
  uint64_t from = std::max(m_read, w > max_n ? w - max_n : 0);
  if(w <= from)
    return;
  const int n = int(w - from);
  m_xs.resize(n);
  m_ys.resize(n);
  if(!ring.read(xc, from, m_xs) || !ring.read(yc, from, m_ys))
  {
    m_read = w;
    return;
  }
  m_read = w;

  const auto r = squareRect();
  m_dots.resize(int(r.width()), int(r.height()));
  const float pers = std::max(persistence(), 0.6f);
  m_dots.decay(pers);

  const bool gonio = mode() == Mode::Goniometer;
  // Auto gain follows the peak quickly up, slowly down
  double peak = 1e-4;
  for(int i = 0; i < n; i++)
  {
    const double a = m_xs[i], b = m_ys[i];
    const double u = gonio ? (a - b) / std::numbers::sqrt2 : a;
    const double v = gonio ? (a + b) / std::numbers::sqrt2 : b;
    peak = std::max({peak, std::abs(u), std::abs(v)});
  }
  m_peak = std::max(peak, m_peak * 0.97);
  const double g = (gonio || autoGain()) ? 0.95 / m_peak : gain();

  const double R = r.width() / 2.;
  const auto style = polar();
  if(gonio && style == Polar::PolarLevel)
  {
    for(auto& b : m_polar_level)
      b *= pers;
    for(int i = 0; i < n; i++)
    {
      const double a = m_xs[i], b = m_ys[i];
      double s = (a - b) / std::numbers::sqrt2, m = (a + b) / std::numbers::sqrt2;
      if(m < 0)
      {
        s = -s;
        m = -m;
      }
      const double ang = std::atan2(-s, m); // [-pi/2, pi/2], left channel at -pi/4
      const int bin = std::clamp(
          int((ang / std::numbers::pi + 0.5) * m_polar_level.size()), 0,
          int(m_polar_level.size()) - 1);
      m_polar_level[bin] += float((s * s + m * m) / n);
    }
    return;
  }

  // Consecutive samples are joined, as the beam of a CRT would; polar
  // samples fold, so they stay dots
  const bool join = !(gonio && style == Polar::PolarSample);
  double px = -1, py = -1;
  for(int i = 0; i < n; i++)
  {
    const double a = m_xs[i], b = m_ys[i];
    double x, y;
    if(!gonio)
    {
      x = R + a * g * R;
      y = R - b * g * R;
    }
    else
    {
      double s = (a - b) / std::numbers::sqrt2, m = (a + b) / std::numbers::sqrt2;
      if(style == Polar::PolarSample)
      {
        // Out of phase samples fold into the upper half
        if(m < 0)
        {
          s = -s;
          m = -m;
        }
      }
      // Left channel up and to the left, right up and to the right
      x = R - s * g * R;
      y = (style == Polar::PolarSample ? 2. * R - 4. : R) - m * g * R;
    }
    if(join && px >= 0)
      m_dots.line(px, py, x, y, 1.f);
    else
      m_dots.hit(x, y, 1.f);
    px = x;
    py = y;
  }
  m_dots.render(QColor(120, 255, 140), 3.f, 0.5f);
}

QRectF Layer::scopeRect() const
{
  return boundingRect().adjusted(28, 20, -4, -14);
}

QRectF Layer::squareRect() const
{
  const auto b = boundingRect().adjusted(4, 20, -4, -24);
  const double side = std::max(10., std::min(b.width(), b.height()));
  return QRectF{b.center().x() - side / 2, b.top(), side, side};
}

void Layer::paint_impl(QPainter* p) const
{
  using namespace Ui::Monitor;
  p->save();
  p->fillRect(boundingRect(), background());
  if(m_channels == 0)
  {
    drawLabel(
        *p, boundingRect(), Qt::AlignCenter,
        QString::fromUtf8(mode_names[int(mode())].data()) + "\nno signal");
    p->restore();
    return;
  }
  switch(mode())
  {
    case Mode::Oscilloscope:
      paintScope(p);
      break;
    case Mode::XY:
      paintDots(p, false);
      break;
    case Mode::Goniometer:
      paintDots(p, true);
      break;
    case Mode::Correlation:
      paintCorrelation(p, boundingRect().adjusted(8, 20, -8, -8));
      break;
  }
  p->restore();
}

void Layer::paintScope(QPainter* p) const
{
  using namespace Ui::Monitor;
  const QRectF r = scopeRect();
  if(r.width() < 4 || r.height() < 4)
    return;
  const double g = currentGain();
  const double mid = r.center().y();

  // Grid: amplitude on the left, time along the bottom
  p->setRenderHint(QPainter::Antialiasing, false);
  const auto y_of = [&](double v) { return mid - v * g * r.height() / 2.; };
  const auto x_of = [&](double ms) { return r.left() + ms / m_window_ms * r.width(); };
  const auto amp = linearTicks(-1. / g, 1. / g, r.height(), 22.);
  const auto time = linearTicks(0., m_window_ms, r.width(), 46., " ms");
  for(const auto& t : amp)
  {
    p->setPen(t.value == 0 ? majorGridColor() : gridColor());
    p->drawLine(QPointF{r.left(), y_of(t.value)}, QPointF{r.right(), y_of(t.value)});
  }
  p->setPen(gridColor());
  for(const auto& t : time)
    p->drawLine(QPointF{x_of(t.value), r.top()}, QPointF{x_of(t.value), r.bottom()});
  drawYAxisLabels(*p, r, amp, y_of);
  drawXAxisLabels(*p, r, time, x_of);

  const float pers = persistence();
  if(pers > 0.f && m_phosphor.size() == m_trace.size())
  {
    for(const auto& img : m_phosphor)
      p->drawImage(r.topLeft(), img.image);
  }
  else
  {
    p->setRenderHint(QPainter::Antialiasing, true);
    for(std::size_t c = 0; c < m_trace.size(); c++)
      drawTrace(p, r, m_trace[c], g, channelColor(int(c)));
  }

  QStringList info;
  info << (m_triggered ? QStringLiteral("trig") : QStringLiteral("free"));
  if(trigger() == Trigger::Pitch && m_period > 2)
    info << QStringLiteral("%1 Hz").arg(m_rate / m_period, 0, 'f', 1);
  if(isUnderMouse())
    drawReadout(*p, QPointF{r.right() - 60, r.top() + 2}, info);
}

//! One trace: a polyline when samples are sparser than pixels, otherwise
//! the min and max of the samples falling in each pixel column.
void Layer::drawTrace(
    QPainter* p, const QRectF& r, const std::vector<float>& x, double g, QColor col)
{
  const int n = int(x.size());
  if(n < 2)
    return;
  const double mid = r.center().y(), half = r.height() / 2.;
  auto y_of = [&](float v) { return std::clamp(mid - v * g * half, r.top(), r.bottom()); };
  p->setPen(QPen(col, 1.2));
  const int cols = int(r.width());
  if(n <= cols)
  {
    QPolygonF poly;
    poly.reserve(n);
    for(int i = 0; i < n; i++)
      poly << QPointF{r.left() + i * r.width() / (n - 1), y_of(x[i])};
    p->drawPolyline(poly);
    return;
  }
  QPolygonF poly;
  poly.reserve(cols * 2);
  for(int c = 0; c < cols; c++)
  {
    const int a = int(std::int64_t(c) * n / cols), b = int(std::int64_t(c + 1) * n / cols);
    float lo = x[a], hi = x[a];
    for(int i = a; i < b; i++)
    {
      lo = std::min(lo, x[i]);
      hi = std::max(hi, x[i]);
    }
    poly << QPointF{r.left() + c, y_of(hi)} << QPointF{r.left() + c, y_of(lo)};
  }
  p->drawPolyline(poly);
}

void Layer::paintDots(QPainter* p, bool gonio) const
{
  using namespace Ui::Monitor;
  const QRectF r = squareRect();
  const double R = r.width() / 2.;
  const QPointF c = r.center();
  const auto style = polar();
  const bool half = gonio && style != Polar::Lissajous;
  const QPointF origin = half ? QPointF{c.x(), r.bottom() - 4} : c;

  p->setRenderHint(QPainter::Antialiasing, true);
  p->setPen(gridColor());
  p->setBrush(Qt::NoBrush);
  if(half)
  {
    p->drawArc(QRectF(origin.x() - R, origin.y() - R, 2 * R, 2 * R), 0, 180 * 16);
    p->drawLine(QPointF{origin.x() - R, origin.y()}, QPointF{origin.x() + R, origin.y()});
  }
  else
  {
    p->drawRect(r);
    p->drawLine(QPointF{r.left(), c.y()}, QPointF{r.right(), c.y()});
    p->drawLine(QPointF{c.x(), r.top()}, QPointF{c.x(), r.bottom()});
  }
  if(gonio)
  {
    // The L, R, M and S axes
    auto axis = [&](double deg, const QString& label) {
      const double a = qDegreesToRadians(deg);
      const QPointF end{origin.x() + R * std::sin(a), origin.y() - R * std::cos(a)};
      p->setPen(majorGridColor());
      p->drawLine(origin, end);
      drawLabel(*p, QRectF(end.x() - 10, end.y() - 12, 20, 12), Qt::AlignCenter, label);
    };
    axis(-45, "L");
    axis(45, "R");
    axis(0, "M");
    axis(-90, "+S");
    axis(90, "-S");
  }

  if(gonio && style == Polar::PolarLevel)
  {
    float mx = 1e-9f;
    for(float v : m_polar_level)
      mx = std::max(mx, v);
    QPolygonF fan;
    fan << origin;
    const int n = int(m_polar_level.size());
    for(int i = 0; i < n; i++)
    {
      const double ang = (double(i) + 0.5) / n * std::numbers::pi - std::numbers::pi / 2;
      const double len = std::sqrt(m_polar_level[i] / mx) * R * 0.95;
      fan << QPointF{origin.x() + len * std::sin(ang), origin.y() - len * std::cos(ang)};
    }
    fan << origin;
    p->setPen(QPen(QColor(120, 255, 140), 1));
    p->setBrush(QColor(120, 255, 140, 80));
    p->drawPolygon(fan);
  }
  else
  {
    p->drawImage(r.topLeft(), m_dots.image);
  }

  if(gonio)
    paintCorrelation(
        p,
        QRectF(r.left(), r.bottom() + 4, r.width(), boundingRect().bottom() - r.bottom() - 6),
        false);
}

void Layer::paintCorrelation(QPainter* p, const QRectF& area, bool history) const
{
  using namespace Ui::Monitor;
  if(area.height() < 6 || area.width() < 20)
    return;
  const double bar_h = history ? std::min(18., area.height() * 0.3) : area.height();
  const QRectF bar{area.left(), area.top(), area.width(), bar_h};
  auto x_of = [&](double k) { return bar.left() + (k + 1.) / 2. * bar.width(); };
  auto col_of = [](double k) {
    return k >= 0.5 ? QColor(76, 175, 80) : k >= 0 ? QColor(240, 200, 40) : QColor(235, 45, 40);
  };

  p->setRenderHint(QPainter::Antialiasing, false);
  p->fillRect(bar, QColor(30, 30, 32));
  for(double k : {-1., -0.5, 0., 0.5, 1.})
  {
    p->setPen(k == 0 ? majorGridColor() : gridColor());
    p->drawLine(QPointF{x_of(k), bar.top()}, QPointF{x_of(k), bar.bottom()});
  }
  const double x0 = x_of(0), xk = x_of(m_corr);
  p->fillRect(
      QRectF(std::min(x0, xk), bar.top() + 2, std::abs(xk - x0), bar.height() - 4),
      col_of(m_corr));
  p->fillRect(QRectF(xk - 1, bar.top(), 2, bar.height()), Qt::white);
  if(bar_h >= 10)
  {
    drawLabel(*p, bar.adjusted(2, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, "-1");
    drawLabel(*p, bar.adjusted(0, 0, -2, 0), Qt::AlignRight | Qt::AlignVCenter, "+1");
    drawLabel(*p, bar, Qt::AlignCenter, QString::number(m_corr, 'f', 2), Qt::white);
  }

  if(!history)
    return;
  // History: newest at the top
  const QRectF hist{
      area.left(), bar.bottom() + 4, area.width(), area.bottom() - bar.bottom() - 4};
  if(hist.height() < 8)
    return;
  p->setPen(gridColor());
  p->drawLine(QPointF{x0, hist.top()}, QPointF{x0, hist.bottom()});
  const int n = int(m_corr_history.size());
  for(int i = 0; i < n; i++)
  {
    const float k = m_corr_history[(m_corr_pos - 1 - i + 2 * n) % n];
    const double y = hist.top() + i * hist.height() / n;
    p->fillRect(
        QRectF(std::min(x0, x_of(k)), y, std::abs(x_of(k) - x0), std::max(1., hist.height() / n)),
        col_of(k));
  }
}
}
