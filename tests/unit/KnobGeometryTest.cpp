// The knob's needle stays inside the dial at every zoom: rendered, and the
// accent pixels measured against the dial's outer edge.

#include <score_test/App.hpp>

#include <score/graphics/widgets/QGraphicsKnob.hpp>
#include <score/model/Skin.hpp>

#include <QImage>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
struct Reach
{
  double needle{}, arc{};
};

//! How far past the dial's outer edge, in device pixels, the accent reaches:
//! near the needle, and along the arc away from it. The arc's own edge is the
//! border the needle must not pass.
Reach overshoot(double side, double scale, double value, double dx, double dy)
{
  score::QGraphicsKnob knob{nullptr};
  knob.setRect(QRectF{0., 0., side, side});
  knob.setValue(value);

  const int w = std::ceil(side * scale) + 4;
  QImage img{w, w, QImage::Format_ARGB32_Premultiplied};
  img.fill(Qt::transparent);
  {
    QPainter p{&img};
    p.translate(dx, dy);
    p.scale(scale, scale);
    QStyleOptionGraphicsItem opt;
    static_cast<QGraphicsItem&>(knob).paint(&p, &opt, nullptr);
  }

  const QColor accent = score::Skin::instance().Base4.main.brush.color();
  const double k = side / 35.;
  const double edge = 11.5 * k * scale;
  const QPointF c{side / 2. * scale + dx, side / 2. * scale + dy};
  // The needle's direction, as the paint code computes it.
  const double space = 50.;
  const double startDeg = 270. - space, spanDeg = 360. - 2. * space;
  const double theta = -(startDeg - value * spanDeg) * M_PI / 180.;
  Reach r;
  for(int y = 0; y < w; y++)
    for(int x = 0; x < w; x++)
    {
      const QColor px = img.pixelColor(x, y);
      if(px.alpha() == 0)
        continue;
      // Only the accent: the dial body is another colour.
      const QColor un = QColor::fromRgbF(
          px.redF() / px.alphaF(), px.greenF() / px.alphaF(), px.blueF() / px.alphaF());
      if(std::abs(un.hslHueF() - accent.hslHueF()) > 0.05 || un.hslSaturationF() < 0.5)
        continue;
      // Past the edge by what the pixel's alpha says of its coverage.
      const double d = std::hypot(x + 0.5 - c.x(), y + 0.5 - c.y());
      if(d < edge - 1.)
        continue;
      const double m = d - edge + px.alphaF() - 0.5;
      const double a = std::atan2(y + 0.5 - c.y(), x + 0.5 - c.x());
      const double da = std::abs(std::remainder(a - theta, 2. * M_PI));
      // Within the needle's width of its direction, or clear of it.
      const double halfAngle = std::atan2(1.5 * k * scale + 1., edge);
      if(da < halfAngle)
        r.needle = std::max(r.needle, m);
      else if(da > 2. * halfAngle)
        r.arc = std::max(r.arc, m);
    }
  return r;
}
}

TEST_CASE("the knob's needle does not pass the dial's edge at any zoom", "[knob]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    // At each zoom, the needle may reach as far as the arc's own antialiased
    // edge does, and no further.
    for(double scale : {0.5, 0.6, 0.75, 1., 1.25, 1.5, 1.75, 2., 2.5, 3., 4.})
    {
      double needle = 0., arc = 0.;
      for(double value : {0., 0.1, 0.25, 0.37, 0.5, 0.63, 0.75, 0.9, 1.})
        for(double off : {0., 0.25, 0.5})
        {
          const auto o = overshoot(35., scale, value, off, off);
          needle = std::max(needle, o.needle);
          arc = std::max(arc, o.arc);
        }
      INFO("scale " << scale << ": needle " << needle << ", arc " << arc);
      CHECK(needle <= arc + 0.05);
    }
  });
}
