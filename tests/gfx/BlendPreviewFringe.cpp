// The edge-blend overlay the multi-window panel draws over each quad.
//
// It is a QPainter gradient, not the render shader, so it has its own way of
// going wrong: QRect::right() and bottom() are the LAST pixel of the rect, not
// the edge past it. A gradient anchored on them stopped one pixel short, and
// the final column and row of the quad kept the unshaded content underneath --
// the one-pixel fully lit fringe along a blended border, which survived the
// shader-side fix because it was never the shader.
//
// Painted here at a size where one pixel is unmistakable.

#include <QImage>
#include <QLinearGradient>
#include <QPainter>

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
struct EdgeBlend
{
  float width{};
  float gamma{1.f};
};

void setGammaGradientStops(QLinearGradient& grad, float gamma)
{
  constexpr int steps = 16;
  for(int i = 0; i <= steps; i++)
  {
    double t = (double)i / steps;
    double overlay = 1.0 - std::pow(t, (double)gamma);
    grad.setColorAt(t, QColor(0, 0, 0, (int)(overlay * 255)));
  }
}

//! The production routine, in both spellings, so the test states the
//! difference rather than describing it.
template <typename R>
void paintRightEdge(QPainter& p, const R& r, const EdgeBlend& right)
{
  p.setPen(Qt::NoPen);
  double w = right.width * r.width();
  QLinearGradient grad(r.right(), r.center().y(), r.right() - w, r.center().y());
  setGammaGradientStops(grad, right.gamma);
  p.setBrush(QBrush(grad));
  p.drawRect(QRectF(r.right() - w, r.top(), w, r.height()));
}

QImage paintedWith(bool useRectF)
{
  QImage img{64, 16, QImage::Format_RGB32};
  img.fill(Qt::white); // the content under the overlay, fully lit
  QPainter p{&img};
  const QRect r{0, 0, 64, 16};
  if(useRectF)
    paintRightEdge(p, QRectF(r), EdgeBlend{0.5f, 1.f});
  else
    paintRightEdge(p, r, EdgeBlend{0.5f, 1.f});
  p.end();
  return img;
}
}

TEST_CASE("the blend overlay reaches the last column of the quad", "[gfx][blend]")
{
  const int y = 8;

  // QRect::right() is the last pixel: the column past it keeps the content.
  const QImage withRect = paintedWith(false);
  CHECK(qRed(withRect.pixel(63, y)) == 255); // the fringe

  // QRectF::right() is the edge: the overlay covers the whole span.
  const QImage withRectF = paintedWith(true);
  CHECK(qRed(withRectF.pixel(63, y)) < 255);

  // And the shading still ramps: dark at the edge, lighter towards the middle.
  CHECK(qRed(withRectF.pixel(63, y)) < qRed(withRectF.pixel(48, y)));
  CHECK(qRed(withRectF.pixel(0, y)) == 255); // the far side is untouched
}
