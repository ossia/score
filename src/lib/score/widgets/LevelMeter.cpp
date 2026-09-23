#include "LevelMeter.hpp"

#include <score/model/Skin.hpp>

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <array>
#include <cmath>

namespace score
{
namespace
{
constexpr float floor_db = -200.f;
constexpr float min_db = -60.f;
constexpr float max_db = 0.f;
constexpr float fall_db_per_second = 20.f;
constexpr double hold_seconds = 2.;
constexpr double layout_seconds = 2.;
constexpr int clip_row = 3;

float to_db(float linear) noexcept
{
  return linear > 1e-10f ? 20.f * std::log10(linear) : floor_db;
}

// Green up to -18 dBFS, yellow up to -6, red above: the usual meter zones.
const QColor& zone_color(float db) noexcept
{
  static const QColor green{76, 196, 112};
  static const QColor yellow{226, 196, 64};
  static const QColor red{226, 72, 58};
  return db < -18.f ? green : db < -6.f ? yellow : red;
}

const QColor& clip_color() noexcept
{
  static const QColor red{240, 40, 30};
  return red;
}
}

LevelMeter::LevelMeter(QWidget* parent)
    : QWidget{parent}
{
  m_clock.start();
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setToolTip(tr("Peak level in dBFS.\nClick to clear the clip marks."));
}

LevelMeter::~LevelMeter() = default;

void LevelMeter::setLevels(std::span<const Channel> channels)
{
  const double now = m_clock.elapsed() / 1000.;
  const double dt = m_active ? std::max(0., now - m_last) : 0.;
  m_last = now;
  m_active = true;

  // Lay out the widest signal of the last seconds.
  const std::size_t n = channels.size();
  if(n >= m_state.size())
  {
    m_state.resize(n);
    m_widestSince = now;
  }
  else if(n > 0 && now - m_widestSince > layout_seconds)
  {
    m_state.resize(n);
    m_widestSince = now;
  }

  const float fall = float(fall_db_per_second * dt);
  for(std::size_t c = 0; c < m_state.size(); c++)
  {
    auto& s = m_state[c];
    const float peak = c < n ? to_db(channels[c].peak) : floor_db;
    const float rms = c < n ? to_db(channels[c].rms) : floor_db;
    if(c < n && channels[c].clipped)
      s.clipped = true;

    s.peak_db = std::max(peak, s.peak_db - fall);
    s.rms_db = std::max(rms, s.rms_db - fall);
    if(peak >= s.hold_db)
    {
      s.hold_db = peak;
      s.hold_since = now;
    }
    else if(now - s.hold_since > hold_seconds)
    {
      s.hold_db = std::max(s.peak_db, s.hold_db - fall);
    }
  }
  update();
}

void LevelMeter::setInactive()
{
  if(!m_active)
    return;
  m_active = false;
  for(auto& s : m_state)
  {
    s.peak_db = floor_db;
    s.rms_db = floor_db;
    s.hold_db = floor_db;
  }
  update();
}

bool LevelMeter::anyClipped() const noexcept
{
  return std::any_of(
      m_state.begin(), m_state.end(), [](const State& s) { return s.clipped; });
}

void LevelMeter::resetClips()
{
  for(auto& s : m_state)
    s.clipped = false;
  update();
}

void LevelMeter::setScaleVisible(bool b)
{
  m_scale = b;
  update();
}

QSize LevelMeter::sizeHint() const
{
  return {24, 120};
}

QSize LevelMeter::minimumSizeHint() const
{
  return {6, 60};
}

void LevelMeter::mousePressEvent(QMouseEvent* e)
{
  resetClips();
  e->accept();
}

void LevelMeter::paintEvent(QPaintEvent*)
{
  auto& skin = score::Skin::instance();
  QPainter p{this};

  p.fillRect(rect(), skin.Background1.color().darker(160));

  const bool scale = m_scale && width() >= 30;
  const int scale_w = scale ? 16 : 0;
  const QRect meter = rect().adjusted(scale_w, 0, 0, 0);
  const QRect bars = meter.adjusted(0, clip_row + 1, 0, 0);
  const double h = bars.height();

  auto y_of = [&](float db) {
    const double t = std::clamp((db - min_db) / (max_db - min_db), 0.f, 1.f);
    return bars.top() + (1. - t) * h;
  };

  if(scale)
  {
    p.setFont(skin.SansFontSmall);
    p.setPen(skin.Gray.color());
    for(int db : {0, -6, -12, -24, -36, -48})
    {
      const int y = int(y_of(float(db)));
      p.drawLine(scale_w - 3, y, scale_w - 1, y);
      p.drawText(
          QRect{0, y - 6, scale_w - 4, 12}, Qt::AlignRight | Qt::AlignVCenter,
          QString::number(-db));
    }
  }

  const int n = int(m_state.size());
  if(n == 0)
    return;

  if(!m_active)
    p.setOpacity(0.35);

  const QColor track = skin.Background1.color().darker(260);
  auto draw_bar = [&](const QRectF& col, const State& s) {
    p.fillRect(col, track);
    const double bottom = col.bottom();
    const double top = y_of(s.peak_db);
    // One rectangle per zone, clipped to the level.
    const std::array<std::pair<float, float>, 3> zones{
        {{min_db, -18.f}, {-18.f, -6.f}, {-6.f, max_db}}};
    for(auto [lo, hi] : zones)
    {
      const double z_bottom = std::min(bottom, y_of(lo));
      const double z_top = std::max(top, y_of(hi));
      if(s.peak_db > lo && z_top < z_bottom)
        p.fillRect(
            QRectF{col.left(), z_top, col.width(), z_bottom - z_top}, zone_color(lo));
    }
    if(s.rms_db > min_db)
      p.fillRect(QRectF{col.left(), y_of(s.rms_db), col.width(), 1.}, skin.Light.color());
    if(s.hold_db > min_db)
      p.fillRect(
          QRectF{col.left(), y_of(s.hold_db) - 1., col.width(), 2.},
          zone_color(s.hold_db));
  };

  const double col_w = double(bars.width()) / n;
  if(col_w >= 3.)
  {
    const double gap = col_w >= 6. ? 1. : 0.;
    for(int c = 0; c < n; c++)
    {
      const auto& s = m_state[c];
      const double x = bars.left() + c * col_w;
      draw_bar(QRectF{x, double(bars.top()), col_w - gap, h}, s);
      p.fillRect(
          QRectF{x, double(meter.top()), col_w - gap, double(clip_row)},
          s.clipped ? clip_color() : track);
    }
  }
  else
  {
    // Too many channels for bars: a heat strip, one column per channel, and
    // one bar for the loudest of them.
    const int loud_w = std::min(6, bars.width() / 3);
    const QRect heat = bars.adjusted(0, 0, -(loud_w + 1), 0);
    const double heat_w = double(heat.width()) / n;
    State loudest;
    bool clipped = false;
    for(int c = 0; c < n; c++)
    {
      const auto& s = m_state[c];
      QColor col = zone_color(s.peak_db);
      const float t = std::clamp((s.peak_db - min_db) / (max_db - min_db), 0.f, 1.f);
      col.setAlphaF(0.12f + 0.88f * t);
      p.fillRect(
          QRectF{heat.left() + c * heat_w, double(heat.top()), std::max(heat_w, 1.), h},
          col);
      loudest.peak_db = std::max(loudest.peak_db, s.peak_db);
      loudest.rms_db = std::max(loudest.rms_db, s.rms_db);
      loudest.hold_db = std::max(loudest.hold_db, s.hold_db);
      clipped |= s.clipped;
    }
    draw_bar(QRectF{double(heat.right() + 2), double(bars.top()), double(loud_w), h}, loudest);
    p.fillRect(
        QRect{meter.left(), meter.top(), meter.width(), clip_row},
        clipped ? clip_color() : track);
  }
}
}
