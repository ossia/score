#include <Scenario/Document/Interval/IntervalPixmaps.hpp>

namespace Scenario
{

void IntervalPixmaps::update(const Process::Style& style)
{
  auto& dashPen = style.IntervalDashPen(style.IntervalBase());
  auto& dashSelectedPen = style.IntervalDashPen(style.IntervalSelected());
  if (oldBase == dashPen.color() && oldSelected == dashSelectedPen.color())
    return;

  static constexpr double dash_width = 18.;
  {
    const auto pen_width = dashPen.widthF();

    QImage image(dash_width, pen_width, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p;
    p.begin(&image);
    p.setPen(dashPen);
    p.drawLine(QPointF{0, pen_width / 2.}, QPointF{dash_width, pen_width / 2.});
    p.end();

    dashed = QPixmap::fromImage(image);
  }

  {
    const auto pen_width = dashSelectedPen.widthF();

    QImage image(dash_width, pen_width, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p;
    p.begin(&image);
    p.setPen(dashSelectedPen);
    p.drawLine(QPointF{0, pen_width / 2.}, QPointF{dash_width, pen_width / 2.});
    p.end();

    dashedSelected = QPixmap::fromImage(image);
  }

  {
    auto dashPlayPen = style.IntervalDashPen(style.IntervalPlayDashFill());
    QColor pulse_base = style.skin.Pulse1.color();
    for (int i = 0; i < 25; i++)
    {
      float alpha = 0.5 + 0.02 * i;
      pulse_base.setAlphaF(alpha);
      dashPlayPen.setColor(pulse_base);

      const auto pen_width = dashSelectedPen.widthF();

      QImage image(dash_width, pen_width, QImage::Format_ARGB32_Premultiplied);
      image.fill(Qt::transparent);
      QPainter p;
      p.begin(&image);
      p.setPen(dashPlayPen);
      p.drawLine(QPointF{0, pen_width / 2.}, QPointF{dash_width, pen_width / 2.});
      p.end();

      playDashed[i] = QPixmap::fromImage(image);
    }
  }
  oldBase = dashPen.color();
  oldSelected = dashSelectedPen.color();
}

void IntervalPixmaps::drawDashes(
    qreal from,
    qreal to,
    QPainter& p,
    const QRectF& visibleRect,
    const QPixmap& pixmap)
{
  from = std::max(from, visibleRect.left());
  to = std::min(to, visibleRect.right());
  const qreal w = pixmap.width();
  const qreal h = -2.;
  for (; from < to - w; from += w)
  {
    p.drawPixmap(from, h, pixmap);
  }

  p.drawPixmap(QRectF{from, h, -1, -1}, pixmap, QRectF{0, 0, to - from, h});
}

IntervalPixmaps& intervalPixmaps(const Process::Style& style)
{
  static IntervalPixmaps pixmaps;
  pixmaps.update(style);
  return pixmaps;
}
}
