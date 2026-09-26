#pragma once
#include <score/model/Skin.hpp>

#include <QPainter>
#include <QRectF>

#include <cmath>

namespace score
{
//! The +/- strip on the right of a combo box or an int spinbox: the upper half
//! steps up, the lower half down.
struct Stepper
{
  static constexpr double width = 12.;

  //! Shown only when the box leaves room for its content next to it.
  static bool visible(const QRectF& box) noexcept { return box.width() > 3. * width; }

  static QRectF rect(const QRectF& box) noexcept
  {
    const double left = std::max(box.left(), box.right() - width);
    return QRectF{QPointF{left, box.top()}, QPointF{box.right(), box.bottom()}};
  }

  //! +1 for the upper half, -1 for the lower half, 0 outside the strip.
  static int stepAt(const QRectF& box, QPointF pos) noexcept
  {
    if(!visible(box))
      return 0;
    const QRectF r = rect(box);
    if(!r.contains(pos))
      return 0;
    return pos.y() < r.center().y() ? +1 : -1;
  }

  //! pressedStep: the half held down (0 for none) while the pointer is on it.
  static void paint(
      QPainter& painter, const score::Skin& skin, const QRectF& box, int pressedStep)
  {
    const QRectF strip = rect(box);
    const QRectF halves[2]
        = {QRectF{strip.topLeft(), QSizeF{strip.width(), strip.height() / 2.}},
           QRectF{
               QPointF{strip.left(), strip.top() + strip.height() / 2.},
               QSizeF{strip.width(), strip.height() / 2.}}};

    // Half the glyph's arm length, so that + and - are the same width.
    const double arm = 2.;
    for(int i = 0; i < 2; i++)
    {
      const int step = i == 0 ? +1 : -1;
      const QRectF& half = halves[i];

      // A pressed half flashes in the text's orange, its glyph in the
      // background colour.
      const bool pressed = pressedStep == step;
      if(pressed)
      {
        painter.setPen(skin.NoPen);
        painter.setBrush(skin.Base4.main.brush);
        painter.drawRect(half);
      }

      // Rounded: a one-pixel pen with antialiasing off needs a whole pixel,
      // and the two glyphs have to line up with each other.
      const QPointF c{std::round(half.center().x()), std::round(half.center().y())};
      painter.setPen(pressed ? skin.Emphasis2.main.pen1 : skin.Base4.main.pen1);
      painter.drawLine(QPointF{c.x() - arm, c.y()}, QPointF{c.x() + arm, c.y()});
      if(step > 0)
        painter.drawLine(QPointF{c.x(), c.y() - arm}, QPointF{c.x(), c.y() + arm});
    }
  }
};
}
