#pragma once
#include <score/model/Skin.hpp>

#include <QColor>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QString>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

/**
 * What every monitor draws with: the same background, grid, text and
 * colour maps, so that meters, scopes and spectra read as one family.
 */
namespace Ui::Monitor
{
struct Tick
{
  double value{};
  QString label;
  bool major{true};
};

inline QColor background()
{
  return QColor(18, 18, 20);
}
inline QColor gridColor()
{
  return QColor(255, 255, 255, 28);
}
inline QColor majorGridColor()
{
  return QColor(255, 255, 255, 48);
}
inline QColor textColor()
{
  return QColor(170, 170, 175);
}
inline QColor traceColor()
{
  return score::Skin::instance().Light.main.brush.color();
}
inline QColor warnColor()
{
  return QColor(255, 196, 0);
}
inline QColor overColor()
{
  return QColor(235, 45, 40);
}

inline const QFont& smallFont()
{
  static const QFont font = [] {
    QFont f = score::Skin::instance().SansFontSmall;
    f.setPixelSize(9);
    return f;
  }();
  return font;
}

//! Colours for the n-th channel or trace, distinct on a dark background.
inline QColor channelColor(int i)
{
  static constexpr std::array<QRgb, 8> c{
      0xff5ec8f2, 0xfff2a65e, 0xff8be07a, 0xffe76f8f,
      0xffb69cf5, 0xfff2e35e, 0xff5ef2c4, 0xffd0d0d0};
  return QColor::fromRgba(c[std::size_t(i) % c.size()]);
}

//! Level colours of a bar meter: green, then yellow from `warn`, red from
//! `over` (all in the meter's own units).
inline QColor zoneColor(double v, double warn, double over)
{
  if(v >= over)
    return QColor(235, 45, 40);
  if(v >= warn)
  {
    const double t = std::clamp((v - warn) / std::max(over - warn, 1e-9), 0., 1.);
    return QColor(int(200 + 50 * t), int(205 - 40 * t), 40);
  }
  return QColor(76, 175, 80);
}

/**
 * The zone colours of zoneColor() as a vertical gradient, `lo` at y_lo and
 * `hi` at y_hi, with the given opacity: a bar of any height is one fill.
 */
inline QLinearGradient
zoneGradient(double y_lo, double y_hi, double lo, double hi, double warn, double over, double alpha = 1.)
{
  QLinearGradient g{QPointF{0., y_lo}, QPointF{0., y_hi}};
  const double span = std::max(hi - lo, 1e-9);
  auto stop = [&](double v, QColor c) {
    c.setAlphaF(alpha);
    g.setColorAt(std::clamp((v - lo) / span, 0., 1.), c);
  };
  stop(lo, zoneColor(lo, warn, over));
  // Each zone edge inside the range: the colour just under it, then over it
  const double eps = 1e-6 * span;
  for(double edge : {warn, over})
    if(edge - eps > lo && edge < hi)
    {
      stop(edge - eps, zoneColor(edge - eps, warn, over));
      stop(edge, zoneColor(edge, warn, over));
    }
  stop(hi, zoneColor(hi, warn, over));
  return g;
}

/**
 * Perceptual colour maps for intensities in [0, 1], built once from the
 * anchor colours of matplotlib's maps.
 */
enum class ColorMap
{
  Inferno,
  Viridis,
  Gray,
  Magma,
  Turbo,
  //! Diverging: blue below the middle, red above
  CoolWarm
};

inline const std::array<QRgb, 256>& colorMap(ColorMap m)
{
  auto build = [](std::initializer_list<std::array<int, 3>> anchors) {
    std::array<QRgb, 256> lut{};
    const std::vector<std::array<int, 3>> a(anchors);
    const int n = int(a.size()) - 1;
    for(int i = 0; i < 256; i++)
    {
      const double t = i / 255. * n;
      const int k = std::min(int(t), n - 1);
      const double f = t - k;
      auto mix = [&](int c) { return int(std::lround(a[k][c] * (1 - f) + a[k + 1][c] * f)); };
      lut[i] = qRgb(mix(0), mix(1), mix(2));
    }
    return lut;
  };
  static const auto inferno = build(
      {{0, 0, 4},
       {40, 11, 84},
       {101, 21, 110},
       {159, 42, 99},
       {212, 72, 66},
       {245, 125, 21},
       {250, 193, 39},
       {252, 255, 164}});
  static const auto viridis = build(
      {{68, 1, 84},
       {70, 50, 127},
       {54, 92, 141},
       {39, 127, 142},
       {31, 161, 135},
       {74, 194, 109},
       {159, 218, 58},
       {253, 231, 37}});
  static const auto gray = build({{0, 0, 0}, {255, 255, 255}});
  static const auto magma = build(
      {{0, 0, 4},
       {28, 16, 68},
       {79, 18, 123},
       {129, 37, 129},
       {181, 54, 122},
       {229, 80, 100},
       {251, 135, 97},
       {254, 194, 135},
       {252, 253, 191}});
  static const auto turbo = build(
      {{48, 18, 59},
       {70, 107, 227},
       {41, 187, 236},
       {49, 242, 153},
       {163, 253, 61},
       {237, 208, 58},
       {251, 128, 34},
       {212, 50, 8},
       {122, 4, 3}});
  static const auto coolwarm
      = build({{59, 76, 192}, {141, 176, 254}, {221, 221, 221}, {244, 154, 123}, {180, 4, 38}});
  switch(m)
  {
    case ColorMap::Viridis:
      return viridis;
    case ColorMap::Gray:
      return gray;
    case ColorMap::Magma:
      return magma;
    case ColorMap::Turbo:
      return turbo;
    case ColorMap::CoolWarm:
      return coolwarm;
    default:
      return inferno;
  }
}

//! "1.5k", "20k", "440"
inline QString formatHz(double hz)
{
  if(hz >= 1000.)
  {
    const double k = hz / 1000.;
    return QString::number(k, 'g', k < 10 ? 2 : 3) + "k";
  }
  return QString::number(hz, 'g', 3);
}

/**
 * Ticks for a linear axis from lo to hi drawn over `pixels`: the finest
 * 1-2-5 step that keeps labels at least `min_spacing` pixels apart.
 */
inline std::vector<Tick>
linearTicks(double lo, double hi, double pixels, double min_spacing = 18., QString unit = {})
{
  std::vector<Tick> ticks;
  if(!(hi > lo) || pixels <= 0)
    return ticks;
  const double span = hi - lo;
  const double raw = span * min_spacing / pixels;
  const double mag = std::pow(10., std::floor(std::log10(raw)));
  double step = mag;
  for(double m : {1., 2., 2.5, 5., 10.})
  {
    step = m * mag;
    if(step >= raw)
      break;
  }
  const double first = std::ceil(lo / step) * step;
  for(double v = first; v <= hi + step * 1e-6; v += step)
  {
    const double r = std::abs(v) < step * 1e-6 ? 0. : v;
    ticks.push_back({r, QString::number(r, 'g', 4) + unit, true});
  }
  return ticks;
}

//! Ticks of a log frequency axis: decades labelled, 2 and 5 in between.
inline std::vector<Tick> frequencyTicks(double lo, double hi, double pixels)
{
  std::vector<Tick> ticks;
  if(lo <= 0 || hi <= lo)
    return ticks;
  const double decades = std::log10(hi / lo);
  const bool dense = pixels / std::max(decades, 1e-3) > 90.;
  for(double dec = std::pow(10., std::floor(std::log10(lo))); dec <= hi; dec *= 10.)
  {
    for(double m : {1., 2., 5.})
    {
      const double f = dec * m;
      if(f < lo || f > hi)
        continue;
      const bool major = m == 1.;
      if(!major && !dense && m == 5.)
        continue;
      ticks.push_back({f, formatHz(f), major});
    }
  }
  return ticks;
}

/**
 * An intensity accumulator for persistent displays (phosphor scopes,
 * goniometers, spectrograms): hits add energy, decay() fades it, render()
 * maps it to colours with a gamma.
 */
struct DensityImage
{
  //! Resolution of the intensities the gamma table maps
  static constexpr int levels = 1024;

  int width{}, height{};
  std::vector<float> acc;
  QImage image;
  //! Intensity level to 0-255 after the gamma, built when the gamma changes
  std::array<uint8_t, levels + 1> gamma_lut{};
  float lut_gamma{-1.f};

  void resize(int w, int h)
  {
    w = std::max(w, 1);
    h = std::max(h, 1);
    if(w == width && h == height)
      return;
    width = w;
    height = h;
    acc.assign(std::size_t(w) * h, 0.f);
    image = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
  }

  void clear() { std::fill(acc.begin(), acc.end(), 0.f); }

  void decay(float factor)
  {
    for(auto& v : acc)
      v *= factor;
  }

  void hit(double x, double y, float energy = 1.f)
  {
    const int ix = int(x), iy = int(y);
    if(ix < 0 || iy < 0 || ix >= width || iy >= height)
      return;
    acc[std::size_t(iy) * width + ix] += energy;
  }

  //! A line of hits from (x0, y0) to (x1, y1), one per pixel step, sharing
  //! `energy` so that fast strokes are dimmer than slow ones, as on a CRT.
  void line(double x0, double y0, double x1, double y1, float energy = 1.f)
  {
    const double dx = x1 - x0, dy = y1 - y0;
    const int n = std::max(1, int(std::ceil(std::max(std::abs(dx), std::abs(dy)))));
    const float e = energy / n;
    for(int i = 0; i <= n; i++)
      hit(x0 + dx * i / n, y0 + dy * i / n, e);
  }

  void buildGamma(float gamma)
  {
    if(gamma == lut_gamma)
      return;
    lut_gamma = gamma;
    for(int i = 0; i <= levels; i++)
      gamma_lut[i] = uint8_t(std::lround(std::pow(float(i) / levels, gamma) * 255.f));
  }

  //! Calls f(row, x, level 0-255) for every pixel, `full` energy at 255.
  template <typename F>
  void map(float full, float gamma, F&& f)
  {
    buildGamma(gamma);
    const float k = levels / std::max(full, 1e-9f);
    for(int y = 0; y < height; y++)
    {
      auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
      const float* a = acc.data() + std::size_t(y) * width;
      for(int x = 0; x < width; x++)
        line[x] = f(gamma_lut[std::size_t(std::min(a[x] * k, float(levels)))]);
    }
  }

  //! Maps the accumulator to `color` at full intensity for `full` energy.
  void render(QColor color, float full, float gamma = 0.5f)
  {
    std::array<QRgb, 256> shade;
    for(int i = 0; i < 256; i++)
      shade[i] = qPremultiply(qRgba(color.red(), color.green(), color.blue(), i));
    map(full, gamma, [&](uint8_t v) { return shade[v]; });
  }

  //! Maps the accumulator through a colour map.
  void render(ColorMap cmap, float full, float gamma = 0.5f)
  {
    const auto& lut = colorMap(cmap);
    map(full, gamma, [&](uint8_t v) { return lut[v]; });
  }
};

//! Draws `text` in the small monitor font, `flags` as for QPainter::drawText.
inline void
drawLabel(QPainter& p, const QRectF& r, int flags, const QString& text, QColor c = textColor())
{
  p.setPen(c);
  p.setFont(smallFont());
  // A single line never runs out of its box: what does not fit is left out
  if(!text.contains(QLatin1Char('\n'))
     && QFontMetricsF{smallFont()}.horizontalAdvance(text) > r.width() + 0.5)
    return;
  p.drawText(r, flags, text);
}

/**
 * Labels of a horizontal axis, under `plot`, at x_of(value): each takes its
 * own width, stays within the plot's width, and is left out when it would
 * touch one already placed. Major ticks are placed first, so that the
 * remaining steps stay even.
 */
template <typename XOf>
void drawXAxisLabels(QPainter& p, const QRectF& plot, const std::vector<Tick>& ticks, XOf x_of)
{
  constexpr double gap = 6.;
  const QFontMetricsF fm{smallFont()};
  std::vector<std::pair<double, double>> taken;
  auto place = [&](const Tick& t) {
    const double w = fm.horizontalAdvance(t.label);
    if(w > plot.width())
      return;
    const double x0 = std::clamp(x_of(t.value) - w / 2., plot.left(), plot.right() - w);
    for(auto [a, b] : taken)
      if(x0 < b + gap && x0 + w > a - gap)
        return;
    taken.emplace_back(x0, x0 + w);
    drawLabel(p, QRectF(x0, plot.bottom() + 1, w + 1, 11), Qt::AlignLeft | Qt::AlignTop, t.label);
  };
  for(const auto& t : ticks)
    if(t.major)
      place(t);
  for(const auto& t : ticks)
    if(!t.major)
      place(t);
}

/**
 * Labels of a vertical axis, right-aligned left of `plot`, at y_of(value):
 * left out when they would touch one already placed, or reach below the plot
 * where a horizontal axis has its labels. Major ticks are placed first.
 */
template <typename YOf>
void drawYAxisLabels(QPainter& p, const QRectF& plot, const std::vector<Tick>& ticks, YOf y_of)
{
  constexpr double h = 10., gap = 3.;
  std::vector<double> taken;
  auto place = [&](const Tick& t) {
    const double y0 = std::clamp(y_of(t.value) - h / 2., plot.top() - h / 2., plot.bottom() - h);
    for(double o : taken)
      if(std::abs(o - y0) < h + gap)
        return;
    taken.push_back(y0);
    drawLabel(
        p, QRectF(0, y0, plot.left() - 3, h), Qt::AlignRight | Qt::AlignVCenter, t.label);
  };
  for(const auto& t : ticks)
    if(t.major)
      place(t);
  for(const auto& t : ticks)
    if(!t.major)
      place(t);
}

//! A readout box in a corner: semi-opaque so that it stays legible over traces.
inline void drawReadout(QPainter& p, QPointF topLeft, const QStringList& lines)
{
  if(lines.isEmpty())
    return;
  p.setFont(smallFont());
  const QFontMetricsF fm{p.font()};
  double w = 0;
  for(auto& l : lines)
    w = std::max(w, fm.horizontalAdvance(l));
  const double lh = fm.height();
  QRectF box{topLeft, QSizeF{w + 6, lh * lines.size() + 4}};
  p.fillRect(box, QColor(0, 0, 0, 150));
  p.setPen(textColor());
  for(int i = 0; i < lines.size(); i++)
    p.drawText(QPointF{box.left() + 3, box.top() + 2 + fm.ascent() + i * lh}, lines[i]);
}
}
