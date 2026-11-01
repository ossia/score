#include "VUMeter.hpp"

#include <Process/Dataflow/Port.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <Ui/Monitor/LayerHelpers.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/small_vector.hpp>

#include <algorithm>
#include <cmath>

namespace Ui::VUMeter
{
Scale scaleFor(Mode m, double floor, double ceiling)
{
  using T = Ui::Monitor::Tick;
  auto dbTicks = [](double lo, double hi) {
    std::vector<T> t;
    for(double d : {12., 9., 6., 3., 0., -3., -6., -9., -12., -18., -24., -30., -36., -42.,
                    -48., -54., -60., -70., -80., -90., -100., -120.})
      if(d >= lo && d <= hi)
        t.push_back({d, d == 0 ? QStringLiteral("0") : QString::number(d)});
    return t;
  };
  switch(m)
  {
    case Mode::VU:
      return {
          -20, 3, -1, 0, "VU",
          {{3, "+3"}, {2, "+2"}, {1, "+1"}, {0, "0"}, {-1, "-1"}, {-2, "-2"}, {-3, "-3"},
           {-5, "-5"}, {-7, "-7"}, {-10, "-10"}, {-20, "-20"}},
          m};
    case Mode::PpmDin:
      return {
          -50, 5, -5, 0, "DIN",
          {{5, "+5"}, {0, "0"}, {-5, "-5"}, {-10, "-10"}, {-20, "-20"}, {-30, "-30"},
           {-40, "-40"}, {-50, "-50"}},
          m};
    case Mode::PpmNordic:
      return {
          -36, 12, 6, 9, "Nordic",
          {{12, "+12"}, {9, "+9"}, {6, "+6"}, {0, "TEST"}, {-6, "-6"}, {-12, "-12"},
           {-18, "-18"}, {-24, "-24"}, {-30, "-30"}, {-36, "-36"}},
          m};
    case Mode::PpmBbc:
      return {
          0, 7.5, 5, 6, "BBC",
          {{7, "7"}, {6, "6"}, {5, "5"}, {4, "4"}, {3, "3"}, {2, "2"}, {1, "1"}},
          m};
    case Mode::PpmEbu:
      return {
          -12, 12, 6, 9, "EBU",
          {{12, "+12"}, {8, "+8"}, {4, "+4"}, {0, "TEST"}, {-4, "-4"}, {-8, "-8"},
           {-12, "-12"}},
          m};
    case Mode::K20:
    case Mode::K14:
    case Mode::K12: {
      const double k = kOffset(m);
      std::vector<T> t;
      for(double d : {k, 12., 8., 4., 0., -4., -8., -12., -20., -30., -40.})
        if(d <= k && (d == k || d < k - 2))
          t.push_back({d, d > 0 ? "+" + QString::number(d) : QString::number(d)});
      return {-40, k, 0, 4, QStringLiteral("K-%1").arg(k), t, m};
    }
    case Mode::TruePeak:
      return {floor, ceiling, -6, -1, "dBTP", dbTicks(floor, ceiling), m};
    default:
      return {floor, ceiling, -12, -3, "dBFS", dbTicks(floor, ceiling), m};
  }
}

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
  m_reference = controlInlet(process, 2);
  m_floor = controlInlet(process, 3);
  m_ceiling = controlInlet(process, 4);
  m_layout = controlInlet(process, 8);
  repaintOn(this, m_reference, m_layout);
  for(auto* c : {m_mode, m_floor, m_ceiling})
    if(c)
      connect(c, &Process::ControlInlet::valueChanged, this, [this] {
        updateScale();
        update();
      });
  updateScale();

  m_link = new Process::MonitorLink{
      process, *this,
      [this](Process::Monitor& m) {
    if(static_cast<MeterMonitor&>(m).frames.consume(m_frame))
      update();
  },
      [this](Process::Monitor*) {
    m_frame = {};
    update();
  },
      this};
}

Mode Layer::mode() const noexcept
{
  return Ui::Monitor::enumValue(m_mode, Mode::PeakRms, Mode::Bits);
}

double Layer::reference() const noexcept
{
  return Ui::Monitor::floatValue(m_reference, -18.f);
}

Ui::Monitor::ChannelLayout Layer::layout() const noexcept
{
  return Ui::Monitor::enumValue(
      m_layout, Ui::Monitor::ChannelLayout::Auto, Ui::Monitor::ChannelLayout::Ambisonic);
}

void Layer::updateScale()
{
  m_scale = scaleFor(
      mode(), Ui::Monitor::floatValue(m_floor, -60.f),
      Ui::Monitor::floatValue(m_ceiling, 0.f));
}

void Layer::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  // The clip indicators along the top clear on a click; elsewhere the
  // click goes to the node, to select or move it
  if(ev->pos().y() >= header && ev->pos().y() < top_margin)
  {
    if(auto* m = m_link->monitor_as<MeterMonitor>())
      m->reset_clips.store(true, std::memory_order_relaxed);
    m_frame.clipped = 0;
    update();
    ev->accept();
    return;
  }
  ev->ignore();
}

void Layer::paint_impl(QPainter* p) const
{
  using namespace Ui::Monitor;
  const auto bounds = boundingRect();
  p->save();
  p->setRenderHint(QPainter::Antialiasing, false);

  const int nch = m_frame.channels;
  if(nch == 0)
  {
    drawLabel(
        *p, bounds, Qt::AlignCenter,
        QString::fromUtf8(mode_names[int(mode())].data()) + "\nno signal");
    p->restore();
    return;
  }

  const Mode m = m_frame.mode;
  QRectF area = bounds.adjusted(0, top_margin, 0, -bottom_margin);
  if(m == Mode::Surround && area.width() > area.height() * 1.4)
  {
    // Polygon on the right, bars on the left
    const double side = std::min(area.height(), area.width() * 0.5);
    QRectF poly{area.right() - side, area.top(), side, side};
    drawSurround(p, poly);
    area.setRight(poly.left() - 6);
  }

  if(m == Mode::Bits)
    drawBits(p, area);
  else if(m == Mode::VU && nch <= 2 && area.width() > 100 && area.height() > 80)
    drawNeedles(p, area);
  else
    drawBars(p, area);

  p->restore();
}

Layer::StripGeometry Layer::strips(const QRectF& area, int nch) const
{
  const double start = area.left() + label_width + 4.;
  const double avail = area.right() - start;
  double sw = 16, sp = 3;
  if(nch * sw + (nch - 1) * sp > avail)
  {
    sp = 1;
    sw = (avail - (nch - 1) * sp) / nch;
    if(sw < 1)
    {
      sp = 0;
      sw = avail / nch;
    }
  }
  else
  {
    // Use the room: wider strips, up to a point
    sw = std::min(48., (avail - (nch - 1) * sp) / nch);
  }
  return {start, sw, sp};
}

void Layer::drawBars(QPainter* p, const QRectF& area) const
{
  using namespace Ui::Monitor;
  const Scale& sc = m_scale;
  const Mode m = m_frame.mode;
  const double ref = reference();
  const int nch = m_frame.channels;
  const double h = area.height();
  if(h <= 4)
    return;

  auto y_of = [&](double v) {
    const double t = std::clamp((v - sc.lo) / (sc.hi - sc.lo), 0., 1.);
    return area.top() + h * (1. - t);
  };
  auto value_of = [&](double lin) { return sc.value(lin, ref); };

  const auto g = strips(area, nch);
  if(g.width <= 0)
    return;
  const bool labels = g.width >= 8;
  const auto lay = layout();

  // As many labels as fit, the roundest values first so that the steps stay
  // even, clear of the unit under the scale
  ossia::small_vector<double, 16> placed;
  {
    auto roundness = [](double v) {
      if(v == 0.)
        return 6;
      for(int k : {24, 12, 6, 3})
        if(std::fmod(std::abs(v), double(k)) == 0.)
          return k == 24 ? 5 : k == 12 ? 4 : k == 6 ? 3 : 2;
      return 1;
    };
    std::vector<const Tick*> order;
    for(const auto& t : sc.ticks)
      order.push_back(&t);
    std::stable_sort(order.begin(), order.end(), [&](const Tick* a, const Tick* b) {
      return roundness(a->value) > roundness(b->value);
    });
    for(const Tick* t : order)
    {
      const double y = y_of(t->value);
      if(y > area.bottom() - 6.)
        continue;
      if(ossia::any_of(placed, [y](double o) { return std::abs(o - y) < 14.; }))
        continue;
      placed.push_back(y);
      drawLabel(
          *p, QRectF(area.left(), y - 5, label_width - 2, 10),
          Qt::AlignRight | Qt::AlignVCenter, t->label);
    }
  }
  drawLabel(
      *p, QRectF(area.left(), area.bottom() + 1, label_width - 2, bottom_margin - 1),
      Qt::AlignRight | Qt::AlignTop, sc.unit);

  // The zone colours over the whole height: each bar is one fill of it
  const QBrush body{zoneGradient(area.bottom(), area.top(), sc.lo, sc.hi, sc.warn, sc.over, 0.55)};
  const QBrush full{zoneGradient(area.bottom(), area.top(), sc.lo, sc.hi, sc.warn, sc.over, 1.)};
  auto bar = [&](double x, double w, double v, const QBrush& brush) {
    if(v <= sc.lo)
      return;
    const double top = y_of(std::min(v, sc.hi));
    p->fillRect(QRectF(x, top, w, area.bottom() - top), brush);
  };

  const bool has_secondary = m == Mode::PeakRms || m == Mode::Surround || m == Mode::TruePeak
                             || m == Mode::K20 || m == Mode::K14 || m == Mode::K12;
  const bool peak_is_main = m == Mode::PeakRms || m == Mode::Surround || m == Mode::TruePeak;
  for(int c = 0; c < nch; c++)
  {
    const double x = g.start + c * (g.width + g.spacing);
    p->fillRect(QRectF(x, area.top(), g.width, h), background());
    // Faint graduations at the labelled values, under the bar
    p->setPen(QColor(255, 255, 255, 14));
    for(double gy : placed)
    {
      const double y = std::round(gy) + 0.5;
      p->drawLine(QPointF{x, y}, QPointF{x + g.width, y});
    }

    const double v = value_of(m_frame.level[c]);
    if(has_secondary)
    {
      // The RMS body is the wide bar, the peak the thin one inside it
      const double vw = value_of(m_frame.secondary[c]);
      const double wide = peak_is_main ? vw : v;
      const double thin = peak_is_main ? v : vw;
      bar(x + 1, std::max(1., g.width - 2), wide, body);
      const double tw = g.width >= 6 ? std::max(1., g.width - 6) : g.width;
      bar(x + (g.width - tw) / 2., tw, thin, full);
    }
    else
    {
      bar(x + 1, std::max(1., g.width - 2), v, full);
    }

    // Peak hold
    const double hv = value_of(m_frame.hold[c]);
    if(hv > sc.lo)
    {
      const double hy = y_of(hv);
      p->fillRect(QRectF(x, hy - 1, g.width, 2), zoneColor(hv, sc.warn, sc.over));
    }

    // Clip indicator
    const bool clipped = m_frame.clipped & (uint64_t(1) << c);
    p->fillRect(
        QRectF(x, header, g.width, top_margin - header - 2),
        clipped ? overColor() : QColor(60, 60, 60));

    if(labels)
    {
      drawLabel(
          *p, QRectF(x - 4, area.bottom() + 1, g.width + 8, bottom_margin - 1),
          Qt::AlignHCenter | Qt::AlignTop, channelLabel(lay, nch, c));
    }
    if(g.width >= 26 && hv > sc.lo)
    {
      drawLabel(
          *p, QRectF(x, area.top(), g.width, 10), Qt::AlignHCenter | Qt::AlignTop,
          QString::number(hv, 'f', 1), QColor(230, 230, 230));
    }
  }
}

void Layer::drawNeedles(QPainter* p, const QRectF& area) const
{
  using namespace Ui::Monitor;
  const int nch = m_frame.channels;
  const double ref = reference();
  // Side by side when there is room for faces wider than tall, else stacked
  const bool row = area.width() / nch >= area.height() * 0.9;
  const double fw = row ? area.width() / nch : area.width();
  const double fh = row ? area.height() : area.height() / nch;
  p->setRenderHint(QPainter::Antialiasing, true);
  for(int c = 0; c < nch; c++)
  {
    const QRectF face
        = QRectF(area.left() + (row ? c * fw : 0), area.top() + (row ? 0 : c * fh), fw, fh)
              .adjusted(3, 3, -3, -3);
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(236, 222, 180));
    p->drawRoundedRect(face, 4, 4);

    // The arc spans 100 degrees; its labels sit just above it
    static constexpr double half_span = 50.;
    const double label_room = 14.;
    double r = std::min(
        (face.width() / 2. - 6.) / std::sin(qDegreesToRadians(half_span)),
        face.height() - label_room - 18.);
    r = std::max(r, 10.);
    const QPointF pivot{face.center().x(), face.top() + label_room + 4. + r};

    // The VU scale is in voltage: -20 VU near the left stop, +3 at the right
    auto angle_of = [](double vu) {
      const double v = ossia::detail::DecibelsToLinearGain(std::clamp(vu, -24., 3.5));
      const double lo = ossia::detail::DecibelsToLinearGain(-20.);
      const double hi = ossia::detail::DecibelsToLinearGain(3.);
      return -half_span + 2. * half_span * (v - lo) / (hi - lo);
    };
    auto at = [&](double deg, double rr) {
      const double a = qDegreesToRadians(deg);
      return QPointF{pivot.x() + rr * std::sin(a), pivot.y() - rr * std::cos(a)};
    };

    p->save();
    p->setClipRect(face);
    QPainterPath arc;
    for(double vu = -20; vu <= 3.01; vu += 0.25)
    {
      const auto pt = at(angle_of(vu), r);
      vu == -20 ? arc.moveTo(pt) : arc.lineTo(pt);
    }
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(QColor(40, 40, 40), 1.2));
    p->drawPath(arc);
    QPainterPath red;
    for(double vu = 0; vu <= 3.01; vu += 0.25)
    {
      const auto pt = at(angle_of(vu), r + 2.);
      vu == 0 ? red.moveTo(pt) : red.lineTo(pt);
    }
    p->setPen(QPen(QColor(200, 30, 30), 3));
    p->drawPath(red);

    p->setFont(smallFont());
    double last_x = -1e9;
    for(double vu : {-20., -10., -7., -5., -3., -2., -1., 0., 1., 2., 3.})
    {
      const double a = angle_of(vu);
      p->setPen(QPen(vu >= 0 ? QColor(200, 30, 30) : QColor(40, 40, 40), 1));
      p->drawLine(at(a, r), at(a, r + 5));
      const auto lp = at(a, r + 11);
      // Skip labels that would overlap the previous one on a small face
      if(lp.x() - last_x < 14.)
        continue;
      last_x = lp.x();
      p->drawText(
          QRectF(lp.x() - 12, lp.y() - 6, 24, 12), Qt::AlignCenter,
          vu > 0 ? "+" + QString::number(vu) : QString::number(vu));
    }

    const double vu = toScale(Mode::VU, m_frame.level[c], ref);
    p->setPen(QPen(QColor(20, 20, 20), 1.5));
    p->drawLine(at(angle_of(vu), r * 0.15), at(angle_of(vu), r + 3));
    p->restore();
    drawLabel(
        *p, face.adjusted(0, 0, 0, -3), Qt::AlignHCenter | Qt::AlignBottom,
        "VU  " + channelLabel(layout(), nch, c), QColor(40, 40, 40));
  }
}

void Layer::drawBits(QPainter* p, const QRectF& area) const
{
  using namespace Ui::Monitor;
  const int nch = m_frame.channels;
  const auto g = strips(area, nch);
  const double rh = area.height() / 24.;
  for(int b = 23; b >= 0; b -= (rh < 7 ? 4 : 1))
  {
    const double y = area.top() + (23 - b) * rh;
    drawLabel(
        *p, QRectF(area.left(), y, label_width - 2, rh), Qt::AlignRight | Qt::AlignVCenter,
        QString::number(24 - b));
  }
  for(int c = 0; c < nch; c++)
  {
    const double x = g.start + c * (g.width + g.spacing);
    for(int b = 0; b < 24; b++)
    {
      const double y = area.top() + (23 - b) * rh;
      const bool on = m_frame.bits[c] & (1u << b);
      p->fillRect(
          QRectF(x, y + 0.5, g.width, std::max(1., rh - 1)),
          on ? QColor(76, 175, 80) : QColor(40, 40, 40));
    }
    if(g.width >= 30)
    {
      drawLabel(
          *p, QRectF(x - 6, area.bottom() + 1, g.width + 12, bottom_margin),
          Qt::AlignHCenter | Qt::AlignTop,
          QStringLiteral("DC %1").arg(m_frame.dc[c] * 100., 0, 'f', 2) + "%");
    }
  }
}

void Layer::drawSurround(QPainter* p, const QRectF& r) const
{
  using namespace Ui::Monitor;
  const int nch = m_frame.channels;
  const auto lay = layout();
  const auto ring = Ui::Monitor::ring(lay, nch);
  const Scale& sc = m_scale;
  p->setRenderHint(QPainter::Antialiasing, true);
  p->fillRect(r, background());
  const QPointF c = r.center();
  const double R = r.width() * 0.42;
  p->setPen(gridColor());
  p->setBrush(Qt::NoBrush);
  for(double f : {0.25, 0.5, 0.75, 1.})
    p->drawEllipse(c, R * f, R * f);

  auto point = [&](int i, double norm) {
    const double a = qDegreesToRadians(ring.azimuth[i]);
    return QPointF{c.x() + R * norm * std::sin(a), c.y() - R * norm * std::cos(a)};
  };
  auto norm_of = [&](int ch) {
    const double v = sc.value(m_frame.secondary[ch], 0);
    return std::clamp((v - sc.lo) / (sc.hi - sc.lo), 0., 1.);
  };

  // Speakers
  for(int i = 0; i < ring.size; i++)
  {
    const auto pt = point(i, 1.12);
    drawLabel(
        *p, QRectF(pt.x() - 14, pt.y() - 6, 28, 12), Qt::AlignCenter,
        channelLabel(lay, nch, ring.channel[i]));
  }
  if(ring.size < 2)
    return;

  // Level polygon, edges coloured by the correlation of the two channels
  QPolygonF poly;
  for(int i = 0; i < ring.size; i++)
    poly << point(i, norm_of(ring.channel[i]));
  p->setPen(Qt::NoPen);
  p->setBrush(QColor(94, 200, 242, 60));
  p->drawPolygon(poly);
  for(int i = 0; i < ring.size; i++)
  {
    const double k = m_frame.ring_correlation[i];
    const QColor col = k >= 0.5   ? QColor(76, 175, 80)
                       : k >= 0.0 ? QColor(240, 200, 40)
                                  : QColor(235, 45, 40);
    p->setPen(QPen(col, 2));
    p->drawLine(poly[i], poly[(i + 1) % ring.size]);
  }
}
}
