#include "Spectrum.hpp"

#include <Process/Dataflow/Port.hpp>

#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <Ui/Monitor/LayerHelpers.hpp>
#include <Ui/Monitor/Paint.hpp>

#include <ossia/network/dataspace/gain.hpp>

namespace Ui::Spectrum
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
  setAcceptHoverEvents(true);
  showInlet(process, 0, doc, this, this);
  ModeSwitch::make(process, doc, this, this);
  m_mode = controlInlet(process, 1);
  m_size = controlInlet(process, 2);
  m_window = controlInlet(process, 3);
  m_avg = controlInlet(process, 4);
  m_slope = controlInlet(process, 5);
  m_smooth = controlInlet(process, 6);
  m_floor = controlInlet(process, 7);
  m_ceiling = controlInlet(process, 8);
  m_channels = controlInlet(process, 9);
  m_hold = controlInlet(process, 10);
  repaintOn(this, m_mode, m_slope, m_smooth, m_floor, m_ceiling, m_hold);
  m_clock.start();

  const auto& lut = colorMap(ColorMap::Inferno);
  m_colour_bar = QImage(int(lut.size()), 1, QImage::Format_RGB32);
  for(int i = 0; i < int(lut.size()); i++)
    reinterpret_cast<QRgb*>(m_colour_bar.scanLine(0))[i] = lut[i];

  m_link = new Process::MonitorLink{
      process, *this,
      [this](Process::Monitor& m) { refresh(static_cast<SpectrumMonitor&>(m)); },
      [this](Process::Monitor*) {
    m_curves.clear();
    m_sonogram = {};
    update();
  }, this};
}

Mode Layer::mode() const
{
  return Ui::Monitor::enumValue(m_mode, Mode::Analyser, Mode::Spectrogram);
}

int Layer::fftSize() const
{
  return 1024 << int(Ui::Monitor::enumValue(m_size, Size::S8192, Size::S32768));
}

double Layer::floorDb() const
{
  return Ui::Monitor::floatValue(m_floor, -100.f);
}

double Layer::ceilingDb() const
{
  return std::max(Ui::Monitor::floatValue(m_ceiling, 0.f), float(floorDb() + 6.));
}

Layer::Display Layer::display() const
{
  using namespace Ui::Monitor;
  return {
      enumValue(m_smooth, Smoothing::Off, Smoothing::Third),
      slopeDbPerOctave(enumValue(m_slope, Slope::Flat, Slope::Tilt45))};
}

void Layer::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  if(!m_doubleClick(*ev))
  {
    ev->ignore();
    return;
  }
  ev->accept();
  if(m_curves.empty())
    return;
  // The levels as drawn, tilt included, over the shown frequencies
  const double slope = display().slope;
  std::vector<double> levels;
  for(const auto& cv : m_curves)
    for(int b = 1; b < int(cv.avg.size()); b++)
    {
      const double f = m_analysis.binFrequency(b);
      if(f < fmin || f > fmax())
        continue;
      levels.push_back(10. * std::log10(cv.avg[b] + 1e-30) + slope * std::log2(f / 1000.));
    }
  if(levels.empty())
    return;
  // Ceiling above the highest, floor at the noise: the lowest few percent are
  // left under it, in 6 dB steps
  const double top = *std::max_element(levels.begin(), levels.end());
  auto low = levels.begin() + levels.size() / 20;
  std::nth_element(levels.begin(), low, levels.end());
  const double ceiling = std::clamp(std::ceil((top + 3.) / 6.) * 6., -60., 24.);
  const double floor = std::clamp(std::floor(*low / 6.) * 6., ceiling - 144., ceiling - 24.);
  Ui::Monitor::setControls(m_stack, {{m_floor, float(std::max(floor, -160.))}, {m_ceiling, float(ceiling)}});
}

void Layer::hoverMoveEvent(QGraphicsSceneHoverEvent* ev)
{
  m_hover = ev->pos();
  update();
}

void Layer::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
  m_hover.reset();
  update();
}

void Layer::refresh(SpectrumMonitor& mon)
{
  using namespace Ui::Monitor;
  auto& ring = mon.ring;
  ring.provision();
  m_rate = ring.rate();
  const int nch = std::min(ring.channels(), 8);
  if(nch == 0)
    return;

  const int n = fftSize();
  const auto win = enumValue(m_window, Window::Hann, Window::FlatTop);
  if(n != m_analysis.size || win != m_analysis.kind || m_rate != m_analysis.rate)
  {
    m_analysis.prepare(n, m_rate, win);
    m_curves.clear();
  }

  // Every channel over the same frames, the latest ones of the first
  m_in.resize(nch);
  for(auto& in : m_in)
    in.resize(n);
  uint64_t first{};
  if(!ring.read_latest(0, m_in[0], first))
    return;
  for(int c = 1; c < nch; c++)
    if(!ring.read(c, first, m_in[c]))
      return;

  // What each curve analyses
  const auto chans = enumValue(m_channels, Channels::Sum, Channels::Right);
  auto mix = [&](auto f) {
    m_mix.resize(n);
    for(int i = 0; i < n; i++)
      m_mix[i] = f(i);
  };
  const int curves = chans == Channels::Each ? nch : chans == Channels::MidSide ? 2 : 1;
  if(int(m_curves.size()) != curves)
    m_curves.assign(curves, {});

  const double dt = std::max(m_clock.restart(), qint64(1)) / 1000.;
  const double avg_s = floatValue(m_avg, 0.3f);
  const double alpha = avg_s <= 0 ? 1. : 1. - std::exp(-dt / avg_s);
  // The held peaks fall at 10 dB/s
  const double hold_fall = ossia::detail::DecibelsToLinearGain(-20. * dt);

  for(int k = 0; k < curves; k++)
  {
    QString name;
    QColor color = channelColor(k);
    switch(chans)
    {
      case Channels::Sum:
        mix([&](int i) {
          float s = 0;
          for(int c = 0; c < nch; c++)
            s += m_in[c][i];
          return s / nch;
        });
        name = "Sum";
        color = traceColor();
        break;
      case Channels::Each:
        m_mix = m_in[k];
        name = QString::number(k + 1);
        break;
      case Channels::MidSide: {
        const int r = std::min(1, nch - 1);
        const float sign = k == 0 ? 1.f : -1.f;
        mix([&](int i) { return (m_in[0][i] + sign * m_in[r][i]) * 0.5f; });
        name = k == 0 ? "M" : "S";
        break;
      }
      case Channels::Left:
        m_mix = m_in[0];
        name = "L";
        break;
      case Channels::Right:
        m_mix = m_in[std::min(1, nch - 1)];
        name = "R";
        break;
    }
    m_analysis.analyse(m_mix.data());
    auto& cv = m_curves[k];
    cv.name = name;
    cv.color = color;
    const auto& p = m_analysis.power;
    if(cv.avg.size() != p.size())
    {
      cv.avg = p;
      cv.peak = p;
    }
    cv.now = p;
    for(std::size_t b = 0; b < p.size(); b++)
    {
      cv.avg[b] += alpha * (p[b] - cv.avg[b]);
      cv.peak[b] = std::max(cv.avg[b], cv.peak[b] * hold_fall);
    }
  }

  if(mode() == Mode::Spectrogram)
  {
    // As many columns as the signal analysed since the last one covers,
    // whatever the refresh rate
    const auto hop = uint64_t(std::max(1., m_rate / columns_per_second));
    const uint64_t end = first + uint64_t(n);
    if(m_column_written == 0 || end < m_column_written || end - m_column_written > 100 * hop)
      m_column_written = end - hop;
    const auto count = int((end - m_column_written) / hop);
    m_column_written += uint64_t(count) * hop;
    addSonogramColumns(count);
  }
  update();
}

//! Level in dB on the display of curve power `power` between two
//! frequencies, tilted and smoothed as asked.
double Layer::levelAt(
    const Display& d, const std::vector<double>& power, double f0, double f1) const
{
  const double fc = std::sqrt(f0 * f1);
  if(d.smoothing != Smoothing::Off)
  {
    const double frac = d.smoothing == Smoothing::Twelfth ? 12.
                        : d.smoothing == Smoothing::Sixth ? 6.
                                                          : 3.;
    f0 = std::min(f0, fc * std::exp2(-0.5 / frac));
    f1 = std::max(f1, fc * std::exp2(0.5 / frac));
  }
  const int n = int(power.size());
  const double b0 = std::clamp(m_analysis.frequencyBin(f0), 0., n - 1.001);
  const double b1 = std::clamp(m_analysis.frequencyBin(f1), b0, n - 1.001);
  auto interp = [&](double b) {
    const int i = int(b);
    const double t = b - i;
    return power[i] * (1 - t) + power[i + 1] * t;
  };
  double v;
  if(d.smoothing != Smoothing::Off)
  {
    // Mean power over the band, the bins at its edges weighed in
    double sum = interp(b0) + interp(b1);
    int count = 2;
    for(int i = int(std::ceil(b0)); i <= int(std::floor(b1)); i++, count++)
      sum += power[i];
    v = sum / count;
  }
  else
  {
    // Highest point of the curve over the pixel: the same rule whether the
    // pixel covers many bins or falls between two
    v = std::max(interp(b0), interp(b1));
    for(int i = int(std::ceil(b0)); i <= int(std::floor(b1)); i++)
      v = std::max(v, power[i]);
  }
  return 10. * std::log10(v + 1e-30) + d.slope * std::log2(fc / 1000.);
}

QRectF Layer::plotRect() const
{
  return boundingRect().adjusted(left_margin, 20, -4, -bottom_margin);
}

double Layer::xOfFreq(const QRectF& r, double f) const
{
  return r.left() + std::log(f / fmin) / std::log(fmax() / fmin) * r.width();
}

double Layer::freqOfX(const QRectF& r, double x) const
{
  return fmin * std::pow(fmax() / fmin, (x - r.left()) / r.width());
}

double Layer::yOfDb(const QRectF& r, double db) const
{
  const double t = std::clamp((db - floorDb()) / (ceilingDb() - floorDb()), 0., 1.);
  return r.bottom() - t * r.height();
}

void Layer::addSonogramColumns(int count)
{
  using namespace Ui::Monitor;
  const QRectF r = plotRect();
  const int w = std::max(8, int(r.width())), h = std::max(8, int(r.height()));
  if(m_sonogram.width() != w || m_sonogram.height() != h)
  {
    m_sonogram = QImage(w, h, QImage::Format_RGB32);
    m_sonogram.fill(colorMap(ColorMap::Inferno)[0]);
    m_column = 0;
  }
  if(m_curves.empty() || count <= 0)
    return;
  count = std::min(count, w);
  const auto& lut = colorMap(ColorMap::Inferno);
  const double lo = floorDb(), hi = ceilingDb();
  const double ratio = fmax() / fmin;
  const Display d = display();
  // Rows from the top: high frequencies up
  for(int y = 0; y < h; y++)
  {
    const double f1 = fmin * std::pow(ratio, double(h - y) / h);
    const double f0 = fmin * std::pow(ratio, double(h - y - 1) / h);
    const double db = levelAt(d, m_curves[0].now, f0, f1);
    const QRgb col = lut[std::clamp(int((db - lo) / (hi - lo) * 255.), 0, 255)];
    auto* line = reinterpret_cast<QRgb*>(m_sonogram.scanLine(y));
    for(int k = 0; k < count; k++)
      line[(m_column + k) % w] = col;
  }
  m_column = (m_column + count) % w;
}

void Layer::paint_impl(QPainter* p) const
{
  using namespace Ui::Monitor;
  p->save();
  p->fillRect(boundingRect(), background());
  if(m_curves.empty())
  {
    drawLabel(
        *p, boundingRect(), Qt::AlignCenter,
        QString::fromUtf8(mode_names[int(mode())].data()) + "\nno signal");
    p->restore();
    return;
  }
  switch(mode())
  {
    case Mode::Analyser:
      paintAnalyser(p);
      break;
    case Mode::RTA:
      paintRta(p);
      break;
    case Mode::Spectrogram:
      paintSonogram(p);
      break;
  }
  p->restore();
}

void Layer::paintGrid(QPainter* p, const QRectF& r) const
{
  using namespace Ui::Monitor;
  p->setRenderHint(QPainter::Antialiasing, false);
  const auto freqs = frequencyTicks(fmin, fmax(), r.width());
  const auto levels = linearTicks(floorDb(), ceilingDb(), r.height(), 18.);
  for(const auto& t : freqs)
  {
    const double x = xOfFreq(r, t.value);
    p->setPen(t.major ? majorGridColor() : gridColor());
    p->drawLine(QPointF{x, r.top()}, QPointF{x, r.bottom()});
  }
  p->setPen(gridColor());
  for(const auto& t : levels)
    p->drawLine(QPointF{r.left(), yOfDb(r, t.value)}, QPointF{r.right(), yOfDb(r, t.value)});
  drawXAxisLabels(*p, r, freqs, [&](double f) { return xOfFreq(r, f); });
  drawYAxisLabels(*p, r, levels, [&](double db) { return yOfDb(r, db); });
}

void Layer::paintAnalyser(QPainter* p) const
{
  using namespace Ui::Monitor;
  const QRectF r = plotRect();
  if(r.width() < 10 || r.height() < 10)
    return;
  paintGrid(p, r);
  p->setRenderHint(QPainter::Antialiasing, true);
  const bool hold = boolValue(m_hold, false);
  const Display d = display();
  const int cols = int(r.width());
  for(const auto& cv : m_curves)
  {
    QPolygonF line, held;
    line.reserve(cols + 2);
    if(hold)
      held.reserve(cols);
    for(int x = 0; x < cols; x++)
    {
      const double f0 = freqOfX(r, r.left() + x), f1 = freqOfX(r, r.left() + x + 1);
      line << QPointF{r.left() + x, yOfDb(r, levelAt(d, cv.avg, f0, f1))};
      if(hold)
        held << QPointF{r.left() + x, yOfDb(r, levelAt(d, cv.peak, f0, f1))};
    }
    QPolygonF fill = line;
    fill << QPointF{r.right(), r.bottom()} << QPointF{r.left(), r.bottom()};
    QColor fc = cv.color;
    fc.setAlpha(m_curves.size() > 1 ? 30 : 60);
    p->setPen(Qt::NoPen);
    p->setBrush(fc);
    p->drawPolygon(fill);
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(cv.color, 1.3));
    p->drawPolyline(line);
    if(hold)
    {
      QColor hc = cv.color;
      hc.setAlpha(140);
      p->setPen(QPen(hc, 1., Qt::DotLine));
      p->drawPolyline(held);
    }
  }
  if(m_curves.size() > 1 && isUnderMouse())
  {
    QStringList names;
    for(auto& c : m_curves)
      names << c.name;
    drawReadout(*p, QPointF{r.right() - 40, r.top() + 2}, names);
  }
  if(m_hover && r.contains(*m_hover))
  {
    const double f = freqOfX(r, m_hover->x());
    const double db = levelAt(d, m_curves[0].avg, f, f * 1.0001);
    p->setPen(QPen(QColor(255, 255, 255, 90), 1, Qt::DashLine));
    p->drawLine(QPointF{m_hover->x(), r.top()}, QPointF{m_hover->x(), r.bottom()});
    drawReadout(
        *p, QPointF{std::min(m_hover->x() + 6, r.right() - 90), r.top() + 2},
        {formatHz(f) + " Hz  " + noteName(f), QString::number(db, 'f', 1) + " dB"});
  }
}

void Layer::paintRta(QPainter* p) const
{
  using namespace Ui::Monitor;
  const QRectF r = plotRect();
  if(r.width() < 10 || r.height() < 10)
    return;
  p->setRenderHint(QPainter::Antialiasing, false);
  const double lo = floorDb(), hi = ceilingDb();
  const auto levels = linearTicks(lo, hi, r.height(), 18.);
  p->setPen(gridColor());
  for(const auto& t : levels)
    p->drawLine(QPointF{r.left(), yOfDb(r, t.value)}, QPointF{r.right(), yOfDb(r, t.value)});
  drawYAxisLabels(*p, r, levels, [&](double db) { return yOfDb(r, db); });
  const int bands = ThirdOctave::count;
  const double bw = r.width() / bands;
  const double tilt_per_oct = display().slope;
  const auto& cv = m_curves[0];
  const QBrush zones{zoneGradient(r.bottom(), r.top(), lo, hi, -12., -3.)};
  // Energy per band from the averaged spectrum
  for(int i = 0; i < bands; i++)
  {
    if(ThirdOctave::lower(i) > m_rate / 2)
      break;
    const double pw = m_analysis.bandPower(cv.avg, ThirdOctave::lower(i), ThirdOctave::upper(i));
    const double db
        = 10. * std::log10(pw + 1e-30) + tilt_per_oct * std::log2(ThirdOctave::centre(i) / 1000.);
    const double x = r.left() + i * bw;
    if(db > lo)
    {
      const double top = yOfDb(r, std::min(db, hi));
      p->fillRect(QRectF(x + 1, top, bw - 2, r.bottom() - top), zones);
    }
  }
  // Band centres by index; octaves (every third band from 31.5 Hz) first
  std::vector<Tick> names;
  for(int i = 0; i < bands && ThirdOctave::lower(i) <= m_rate / 2; i++)
    names.push_back({double(i), QString::fromLatin1(ThirdOctave::labels[i]), i % 3 == 2});
  drawXAxisLabels(*p, r, names, [&](double i) { return r.left() + (i + 0.5) * bw; });
}

void Layer::paintSonogram(QPainter* p) const
{
  using namespace Ui::Monitor;
  const QRectF r = plotRect();
  if(m_sonogram.isNull())
    return;
  // Oldest column on the left
  const int w = m_sonogram.width(), h = m_sonogram.height();
  const int right = w - m_column;
  p->drawImage(QRectF(r.left(), r.top(), right, h), m_sonogram, QRectF(m_column, 0, right, h));
  p->drawImage(
      QRectF(r.left() + right, r.top(), m_column, h), m_sonogram, QRectF(0, 0, m_column, h));
  // Frequency axis on the left
  const auto freqs = frequencyTicks(fmin, fmax(), r.height());
  auto y_of = [&](double f) {
    return r.bottom() - std::log(f / fmin) / std::log(fmax() / fmin) * r.height();
  };
  p->setPen(QColor(255, 255, 255, 40));
  for(const auto& t : freqs)
    p->drawLine(QPointF{r.left(), y_of(t.value)}, QPointF{r.left() + 6, y_of(t.value)});
  drawYAxisLabels(*p, r, freqs, y_of);
  // Colour scale along the bottom
  const QRectF bar{r.left(), r.bottom() + 3, std::min(120., r.width()), 6};
  p->drawImage(bar, m_colour_bar);
  drawLabel(
      *p, QRectF(bar.right() + 4, bar.top() - 3, 120, 12), Qt::AlignLeft | Qt::AlignVCenter,
      QStringLiteral("%1 to %2 dB").arg(floorDb()).arg(ceilingDb()));
}
}
