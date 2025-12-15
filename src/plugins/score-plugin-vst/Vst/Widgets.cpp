#include "Widgets.hpp"

#include <Process/Style/Pixmaps.hpp>

#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Dataflow/Commands/CreateModulation.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Effect/EffectLayout.hpp>
#include <Engine/Node/CommonWidgets.hpp>
#include <Vst/Commands.hpp>
#include <Vst/Control.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/Pixmap.hpp>

#include <ossia/detail/ssize.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QGraphicsScene>

#include <wobjectimpl.h>

W_OBJECT_IMPL(vst::GraphicsSlider)
namespace vst
{

GraphicsSlider::GraphicsSlider(AEffect* fx, int num, QGraphicsItem* parent)
    : QGraphicsSliderBase{parent}
{
  this->fx = fx;
  this->num = num;
  if(!fx)
    return;

  this->m_value = fx->getParameter(fx, num);
}

void GraphicsSlider::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

void GraphicsSlider::setExecutionValue(const ossia::value& v)
{
  m_execValue = ossia::clamp(ossia::convert<float>(v), 0.f, 1.f);
  m_hasExec = true;
  update();
}

void GraphicsSlider::resetExecution()
{
  m_hasExec = false;
  update();
}

double GraphicsSlider::value() const
{
  return m_value;
}

void GraphicsSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  score::DefaultGraphicsSliderImpl::mousePressEvent(*this, event);
}

void GraphicsSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  score::DefaultGraphicsSliderImpl::mouseMoveEvent(*this, event);
}

void GraphicsSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  score::DefaultGraphicsSliderImpl::mouseReleaseEvent(*this, event);
}

void GraphicsSlider::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  score::DefaultGraphicsSliderImpl::mouseDoubleClickEvent(*this, event);
}

void GraphicsSlider::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if(!fx)
    return;

  char str[256]{};
  // Note that effGetParamDisplay will sometimes ignore the m_value argument
  // and just return the value for the actual parameter, which may be different
  // when executing...
  fx->dispatcher(fx, effGetParamDisplay, num, 0, str, m_value);

  m_execValue = fx->getParameter(fx, num);
  score::DefaultGraphicsSliderImpl::paint(
      *this, score::Skin::instance(), QString::fromUtf8(str), painter, widget);
}

QWidget* VSTFloatSlider::make_widget(
    AEffect* fx, const ControlInlet& inlet, const score::DocumentContext& ctx,
    QWidget* parent, QObject* context)
{
  auto sl = new score::ValueDoubleSlider{parent};
  sl->min = 0.;
  sl->max = 1.;
  sl->setValue(ossia::convert<double>(inlet.value()));

  QObject::connect(
      sl, &score::ValueDoubleSlider::sliderMoved, context, [=, &inlet, &ctx] {
        sl->moving = true;
        ctx.dispatcher.submit<SetControl>(inlet, sl->value());
      });
  QObject::connect(sl, &score::ValueDoubleSlider::sliderReleased, context, [&ctx, sl]() {
    ctx.dispatcher.commit();
    sl->moving = false;
  });

  QObject::connect(
      &inlet, &ControlInlet::valueChanged, sl, [=](const ossia::value& val) {
    if(!sl->moving)
      sl->setValue(ossia::convert<float>(val));
  });

  return sl;
}
QGraphicsItem* VSTFloatSlider::make_item(
    AEffect* fx, ControlInlet& inlet, const score::DocumentContext& ctx,
    QGraphicsItem* parent, QObject* context)
{
  auto sl = new GraphicsSlider{fx, inlet.fxNum, parent};
  sl->setValue(ossia::convert<double>(inlet.value()));

  QObject::connect(sl, &GraphicsSlider::sliderMoved, context, [=, &inlet, &ctx] {
    sl->moving = true;
    ctx.dispatcher.submit<SetControl>(inlet, sl->value());
  });
  QObject::connect(sl, &GraphicsSlider::sliderReleased, context, [&ctx, sl]() {
    ctx.dispatcher.commit();
    sl->moving = false;
  });

  QObject::connect(
      &inlet, &ControlInlet::valueChanged, sl, [=](const ossia::value& val) {
    if(!sl->moving)
      sl->setValue(ossia::convert<float>(val));
  });

  QObject::connect(
      &inlet, &ControlInlet::executionValueChanged, sl,
      &GraphicsSlider::setExecutionValue);
  QObject::connect(
      &inlet, &ControlInlet::executionReset, sl, &GraphicsSlider::resetExecution);

  return sl;
}
}
