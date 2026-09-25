#include "WidgetPresentation.hpp"

#include <score/graphics/widgets/Constants.hpp>
#include <score/graphics/widgets/QGraphicsIntSlider.hpp>
#include <score/graphics/widgets/QGraphicsKnob.hpp>
#include <score/graphics/widgets/QGraphicsLogKnob.hpp>
#include <score/graphics/widgets/QGraphicsLogSlider.hpp>
#include <score/graphics/widgets/QGraphicsSlider.hpp>

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
}

void setValueOnHover(QGraphicsItem& control, bool onHover)
{
  if(!isKnob(control) && !isSlider(control))
    return;
  control.setData(ValueOnHoverDataKey, onHover);
  // QGraphicsItem's own hover handlers repaint on enter and leave.
  control.setAcceptHoverEvents(control.acceptHoverEvents() || onHover);
  control.update();
}

void setControlSize(QGraphicsItem& control, ControlSize size)
{
  if(size == ControlSize::Normal)
    return;
  const bool isSmall = size == ControlSize::Small;
  // Small knobs are too small to draw their value inside: it goes below
  const QRectF knob = isSmall ? QRectF{0., 0., 30., 41.} : QRectF{0., 0., 48., 59.};
  if(auto k = dynamic_cast<QGraphicsKnob*>(&control))
    k->setRect(knob);
  else if(auto k = dynamic_cast<QGraphicsLogKnob*>(&control))
    k->setRect(knob);
  else if(isSlider(control))
  {
    // Sliders keep their height, which is their value text's
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
