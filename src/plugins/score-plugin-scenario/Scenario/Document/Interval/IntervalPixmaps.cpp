#include <Scenario/Document/Interval/IntervalPixmaps.hpp>

namespace Scenario
{

void IntervalPixmaps::update(const Process::Style& style)
{
  if (loadIndex == style.skin.LoadIndex)
    return;
  loadIndex = style.skin.LoadIndex;

  auto& dashPen = style.IntervalDashPen(style.IntervalBase());
  auto& dashSelectedPen = style.IntervalDashPen(style.IntervalSelected());
  auto& dashDropTargetPen = style.IntervalDashPen(style.IntervalDropTarget());
  auto& dashWarningPen = style.IntervalDashPen(style.IntervalWarning());
  auto& dashLoopPen = style.IntervalDashPen(style.IntervalLoop());
  auto& dashMutedPen = style.IntervalDashPen(style.IntervalMuted());

  const double pen_width = dashPen.widthF();
  static constexpr double dash_width = 18.;

  QImage image(dash_width, pen_width, QImage::Format_ARGB32_Premultiplied);

  for(auto& [pen, pixmap] : {
      std::tie(dashPen, dashed),
      std::tie(dashSelectedPen, dashedSelected),
      std::tie(dashDropTargetPen, dashedDropTarget),
      std::tie(dashWarningPen, dashedWarning),
      std::tie(dashLoopPen, dashedInvalid),
      std::tie(dashMutedPen, dashedMuted)
    })
  {
    image.fill(Qt::transparent);
    QPainter p;
    p.begin(&image);
    p.setPen(pen);
    p.drawLine(QPointF{0, pen_width / 2.}, QPointF{dash_width, pen_width / 2.});
    p.end();

    pixmap = QPixmap::fromImage(image);
  }

  {
    auto dashPlayPen = style.IntervalDashPen(style.IntervalPlayDashFill());
    QColor pulse_base = style.skin.Pulse1.color();
    for (int i = 0; i < 25; i++)
    {
      float alpha = 0.5 + 0.02 * i;
      pulse_base.setAlphaF(alpha);
      dashPlayPen.setColor(pulse_base);

      image.fill(Qt::transparent);
      QPainter p;
      p.begin(&image);
      p.setPen(dashPlayPen);
      p.drawLine(QPointF{0, pen_width / 2.}, QPointF{dash_width, pen_width / 2.});
      p.end();

      playDashed[i] = QPixmap::fromImage(image);
    }
  }
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
  const qreal h = -1.;
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
