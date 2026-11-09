#include "WidgetPresentation.hpp"

#include <score/graphics/widgets/Constants.hpp>
#include <score/graphics/widgets/QGraphicsIntSlider.hpp>
#include <score/graphics/widgets/QGraphicsKnob.hpp>
#include <score/graphics/widgets/QGraphicsLogKnob.hpp>
#include <score/graphics/widgets/QGraphicsLogSlider.hpp>
#include <score/graphics/widgets/QGraphicsSlider.hpp>

#include <QFontMetricsF>

#include <algorithm>

namespace score
{
namespace
{
bool isKnob(QGraphicsItem& c)
{
  return dynamic_cast<QGraphicsKnob*>(&c) || dynamic_cast<QGraphicsLogKnob*>(&c);
}
bool isSlider(QGraphicsItem& c)
{
  return dynamic_cast<QGraphicsSlider*>(&c) || dynamic_cast<QGraphicsLogSlider*>(&c)
         || dynamic_cast<QGraphicsIntSlider*>(&c);
}

//! A font and the ones a pixel, two and three pixels smaller, with the width
//! a character takes in each: in a long value, mostly digits, with maybe a
//! dot or a space; in a short one, maybe a word
struct ValueFonts
{
  static constexpr int count = 4;
  static constexpr int longText = 10;
  QFont base;
  QFont fonts[count];
  qreal digit[count]{};
  qreal character[count]{};

  void reset(const QFont& f)
  {
    base = f;
    const int px = f.pixelSize() > 0 ? f.pixelSize() : 10;
    for(int i = 0; i < count; i++)
    {
      fonts[i] = f;
      if(i > 0)
        fonts[i].setPixelSize(std::max(1, px - i));
      const QFontMetricsF metrics{fonts[i]};
      digit[i] = metrics.horizontalAdvance(QLatin1Char('0'));
      character[i] = std::max(digit[i], metrics.averageCharWidth());
    }
  }
};
}

FittedText fitValueText(const QFont& font, const QString& text, qreal width)
{
  // Only painted from the GUI thread, almost always with the same font
  static ValueFonts f;
  if(f.base != font || f.digit[0] <= 0.)
    f.reset(font);

  const auto n = text.size();
  const auto& perChar = n >= ValueFonts::longText ? f.digit : f.character;
  for(int i = 0; i < ValueFonts::count; i++)
    if(n * perChar[i] <= width)
      return {&f.fonts[i], text};

  // A character to spare: what is cut is no longer mostly digits
  constexpr int smallest = ValueFonts::count - 1;
  const auto fit = std::max(qsizetype(1), qsizetype(width / perChar[smallest]) - 1);
  return {&f.fonts[smallest], text.left(fit)};
}

void setValueOnHover(QGraphicsItem& control, bool onHover)
{
  if(!isKnob(control) && !isSlider(control))
    return;
  control.setData(ValueOnHoverDataKey, onHover);
  // Default hover handlers repaint on enter/leave
  control.setAcceptHoverEvents(control.acceptHoverEvents() || onHover);
  control.update();
}

void setValueAlignment(QGraphicsItem& control, Qt::Alignment align)
{
  if(!isKnob(control))
    return;
  control.setData(ValueAlignmentDataKey, int(align & Qt::AlignHorizontal_Mask));
  control.update();
}

void setControlSize(QGraphicsItem& control, ControlSize size)
{
  if(size == ControlSize::Normal)
    return;
  const bool isSmall = size == ControlSize::Small;
  // Value drawn below the knob
  const QRectF knob = isSmall ? QRectF{0., 0., 30., 41.} : QRectF{0., 0., 48., 59.};
  if(auto k = dynamic_cast<QGraphicsKnob*>(&control))
    k->setRect(knob);
  else if(auto k = dynamic_cast<QGraphicsLogKnob*>(&control))
    k->setRect(knob);
  else if(isSlider(control))
  {
    // Sliders keep their height
    const QRectF r = defaultSliderSize;
    const double w = isSmall ? 44. : 90.;
    if(auto s = dynamic_cast<QGraphicsSlider*>(&control))
      s->setRect({0., 0., w, r.height()});
    else if(auto s = dynamic_cast<QGraphicsLogSlider*>(&control))
      s->setRect({0., 0., w, r.height()});
    else if(auto s = dynamic_cast<QGraphicsIntSlider*>(&control))
      s->setRect({0., 0., w, r.height()});
  }
}
}
