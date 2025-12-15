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
#include <Vst3/Commands.hpp>
#include <Vst3/Control.hpp>

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

W_OBJECT_IMPL(vst3::VSTGraphicsSlider)

namespace vst3
{

VSTGraphicsSlider::VSTGraphicsSlider(
    Steinberg::Vst::IEditController* fx, Steinberg::Vst::ParamID num,
    QGraphicsItem* parent)
    : QGraphicsSliderBase{parent}
{
  this->fx = fx;
  this->num = num;
  if(fx)
    this->m_value = fx->getParamNormalized(num);
}

void VSTGraphicsSlider::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

void VSTGraphicsSlider::setExecutionValue(const ossia::value& v)
{
  m_execValue = ossia::clamp(ossia::convert<double>(v), 0., 1.);
  m_hasExec = true;
  update();
}

void VSTGraphicsSlider::resetExecution()
{
  m_hasExec = false;
  update();
}

double VSTGraphicsSlider::value() const
{
  return m_value;
}

void VSTGraphicsSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  score::DefaultGraphicsSliderImpl::mousePressEvent(*this, event);
}

void VSTGraphicsSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  score::DefaultGraphicsSliderImpl::mouseMoveEvent(*this, event);
}

void VSTGraphicsSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  score::DefaultGraphicsSliderImpl::mouseReleaseEvent(*this, event);
}

void VSTGraphicsSlider::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  score::DefaultGraphicsSliderImpl::mouseDoubleClickEvent(*this, event);
}

void VSTGraphicsSlider::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if(!fx)
    return;

  Steinberg::Vst::String128 str = {};
  m_value = fx->getParamNormalized(num);
  fx->getParamStringByValue(num, m_value, str);

  score::DefaultGraphicsSliderImpl::paint(
      *this, score::Skin::instance(), fromString(str), painter, widget);
}

QWidget* VSTFloatSlider::make_widget(
    Steinberg::Vst::IEditController* fx, const ControlInlet& inlet,
    const score::DocumentContext& ctx, QWidget* parent, QObject* context)
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
      &inlet, &vst3::ControlInlet::valueChanged, sl, [=](const ossia::value& val) {
    if(!sl->moving)
      sl->setValue(ossia::convert<float>(val));
  });

  return sl;
}
QGraphicsItem* VSTFloatSlider::make_item(
    Steinberg::Vst::IEditController* fx, ControlInlet& inlet,
    const score::DocumentContext& ctx, QGraphicsItem* parent, QObject* context)
{
  auto sl = new VSTGraphicsSlider{fx, inlet.fxNum, parent};
  sl->init = ossia::convert<double>(inlet.init());
  sl->setValue(ossia::convert<double>(inlet.value()));

  QObject::connect(sl, &VSTGraphicsSlider::sliderMoved, context, [=, &inlet, &ctx] {
    sl->moving = true;
    ctx.dispatcher.submit<SetControl>(inlet, sl->value());
  });
  QObject::connect(sl, &VSTGraphicsSlider::sliderReleased, context, [&ctx, sl]() {
    ctx.dispatcher.commit();
    sl->moving = false;
  });

  QObject::connect(
      &inlet, &vst3::ControlInlet::valueChanged, sl, [=](const ossia::value& val) {
    if(!sl->moving)
      sl->setValue(ossia::convert<float>(val));
  });

  QObject::connect(
      &inlet, &vst3::ControlInlet::executionValueChanged, sl,
      &VSTGraphicsSlider::setExecutionValue);

  QObject::connect(
      &inlet, &vst3::ControlInlet::executionReset, sl,
      &VSTGraphicsSlider::resetExecution);
  return sl;
}
}
