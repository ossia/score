#pragma once
#include <score/model/Skin.hpp>

#include <QPainter>
#include <QRectF>

#include <cmath>

namespace score
{
//! The +/- strip on the right of a combo box or an int spinbox: the upper half
//! steps up, the lower half down. In a box too short for two halves one above
//! the other, they sit side by side, - then +.
struct Stepper
{
  static constexpr double width = 12.;
  //! Below this height the halves sit side by side
  static constexpr double min_stacked_height = 18.;

  static bool sideBySide(const QRectF& box) noexcept
  {
    return box.height() < min_stacked_height;
  }

  //! Width of the strip in that box.
  static double stripWidth(const QRectF& box) noexcept
  {
    return sideBySide(box) ? 2. * width : width;
  }

  //! Shown only when the box leaves room for its content next to it.
  static bool visible(const QRectF& box) noexcept
  {
    return box.width() > stripWidth(box) + 2. * width;
  }

  static QRectF rect(const QRectF& box) noexcept
  {
    const double left = std::max(box.left(), box.right() - stripWidth(box));
    return QRectF{QPointF{left, box.top()}, QPointF{box.right(), box.bottom()}};
  }

  //! The half that steps by `step`: +1 or -1.
  static QRectF half(const QRectF& box, int step) noexcept
  {
    const QRectF strip = rect(box);
    if(sideBySide(box))
      return step > 0 ? QRectF{QPointF{strip.center().x(), strip.top()}, strip.bottomRight()}
                      : QRectF{strip.topLeft(), QPointF{strip.center().x(), strip.bottom()}};
    return step > 0 ? QRectF{strip.topLeft(), QPointF{strip.right(), strip.center().y()}}
                    : QRectF{QPointF{strip.left(), strip.center().y()}, strip.bottomRight()};
  }

  //! +1 or -1 for the half under `pos`, 0 outside the strip.
  static int stepAt(const QRectF& box, QPointF pos) noexcept
  {
    if(!visible(box))
      return 0;
    if(half(box, +1).contains(pos))
      return +1;
    if(half(box, -1).contains(pos))
      return -1;
    return 0;
  }

  //! pressedStep: the half held down (0 for none) while the pointer is on it.
  static void paint(
      QPainter& painter, const score::Skin& skin, const QRectF& box, int pressedStep)
  {
    const QRectF halves[2] = {half(box, +1), half(box, -1)};

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
