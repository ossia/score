#include "Widgets.hpp"

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Dataflow/Commands/CreateModulation.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Effect/EffectLayout.hpp>
#include <Engine/Node/CommonWidgets.hpp>
#include <Process/Style/Pixmaps.hpp>
#include <Vst3/Commands.hpp>
#include <Vst3/Control.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QGraphicsScene>

#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(vst3::VSTGraphicsSlider)

namespace vst3
{

VSTGraphicsSlider::VSTGraphicsSlider(
    Steinberg::Vst::IEditController* fx,
    Steinberg::Vst::ParamID num,
    QGraphicsItem* parent)
    : QGraphicsSliderBase{parent}
{
  this->fx = fx;
  this->num = num;
  this->m_value = fx->getParamNormalized(num);
  this->setAcceptedMouseButtons(Qt::LeftButton);
}

void VSTGraphicsSlider::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

void VSTGraphicsSlider::setExecutionValue(double v)
{
  m_execValue = ossia::clamp(v, 0., 1.);
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
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  Steinberg::Vst::String128 str;
  m_value = fx->getParamNormalized(num);
  fx->getParamStringByValue(num, m_value, str);

  score::DefaultGraphicsSliderImpl::paint(
      *this, score::Skin::instance(), fromString(str), painter, widget);
}

VSTEffectItem::VSTEffectItem(
    const Model& effect,
    const Process::Context& doc,
    QGraphicsItem* root)
    : score::EmptyRectItem{root}
{
  rootItem = root;

  if (!effect.fx)
    return;

  using namespace Control::Widgets;
  QObject::connect(
      &effect,
      &Process::ProcessModel::controlAdded,
      this,
      [&](const Id<Process::Port>& id) {
        auto inlet = safe_cast<ControlInlet*>(effect.inlet(id));
        setupInlet(effect, *inlet, doc);
      });

  QObject::connect(
      &effect,
      &Process::ProcessModel::controlRemoved,
      this,
      [&](const Process::Port& port) {
        auto inlet = qobject_cast<const ControlInlet*>(&port);
        SCORE_ASSERT(inlet);
        auto it = ossia::find_if(
            controlItems, [&](auto p) { return p.first == inlet; });
        if (it != controlItems.end())
        {
          delete it->second;
          it = controlItems.erase(it);
          int i = std::distance(controlItems.begin(), it);
          for (; it != controlItems.end(); ++it, ++i)
          {
            score::EmptyRectItem* rect = it->second;
            QPointF pos = Process::currentWigetPos(i, [&](int j) {
              return controlItems[j].second->boundingRect().size();
            });

            rect->setPos(pos);
          }
        }
        updateRect();
      });

  for (auto inlet : effect.inlets())
  {
    if (auto inl = qobject_cast<ControlInlet*>(inlet))
      setupInlet(effect, *inl, doc);
  }
  updateRect();
}

void VSTEffectItem::setupInlet(
    const Model& fx,
    ControlInlet& inlet,
    const Process::Context& doc)
{
  if (!fx.fx)
    return;

  int i = controlItems.size();

  auto csetup = Process::controlSetup(
      [](auto& factory, auto& inlet, const auto& doc, auto item, auto parent) {
        return factory.makeItem(inlet, doc, item, parent);
      },
      [&](auto& factory,
          auto& inlet,
          const auto& doc,
          auto item,
          auto parent) {
        return VSTFloatSlider::make_item(
            fx.fx.controller, inlet, doc, item, parent);
      },
      [&](int j) { return controlItems[j].second->boundingRect().size(); },
      [&] { return inlet.name(); },
      [](auto&&...) -> auto& {
        static VSTControlPortFactory f;
        return f;
      });

  // TODO useless, find a way to remove
  static const auto& portFactory
      = score::GUIAppContext().interfaces<Process::PortFactoryList>();
  auto ctl
      = Process::createControl(i, csetup, inlet, portFactory, doc, this, this);

  if (fx.fx.controller->getParameterCount() >= VST_DEFAULT_PARAM_NUMBER_CUTOFF)
  {
    const auto& pixmaps = Process::Pixmaps::instance();
    auto rm_item = new score::QGraphicsPixmapButton{
        pixmaps.close_on, pixmaps.close_off, ctl.item};
    connect(
        rm_item,
        &score::QGraphicsPixmapButton::clicked,
        this,
        [&doc, &fx, id = inlet.id()] {
          QTimer::singleShot(0, [&doc, &fx, id] {
            CommandDispatcher<> disp{doc.commandStack};
            SCORE_TODO_("FIXME: implement vst3 control removal");
            // disp.submit<RemoveVSTControl>(fx, id);
          });
        });

    rm_item->setPos(8., 16.);
  }
  controlItems.push_back({&inlet, ctl.item});
  updateRect();
}

void VSTEffectItem::updateRect()
{
  QRectF cr = childrenBoundingRect();
  cr.setWidth(std::max(100., cr.width()));
  cr.setHeight(std::max(10., cr.height()));
  setRect(cr);
}

QWidget* VSTFloatSlider::make_widget(
    Steinberg::Vst::IEditController* fx,
    const ControlInlet& inlet,
    const score::DocumentContext& ctx,
    QWidget* parent,
    QObject* context)
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
  QObject::connect(
      sl, &score::ValueDoubleSlider::sliderReleased, context, [&ctx, sl]() {
        ctx.dispatcher.commit();
        sl->moving = false;
      });

  QObject::connect(
      &inlet, &vst3::ControlInlet::valueChanged, sl, [=](float val) {
        if (!sl->moving)
          sl->setValue(val);
      });

  return sl;
}
QGraphicsItem* VSTFloatSlider::make_item(
    Steinberg::Vst::IEditController* fx,
    ControlInlet& inlet,
    const score::DocumentContext& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  auto sl = new VSTGraphicsSlider{fx, inlet.fxNum, parent};
  sl->setValue(ossia::convert<double>(inlet.value()));

  QObject::connect(
      sl, &VSTGraphicsSlider::sliderMoved, context, [=, &inlet, &ctx] {
        sl->moving = true;
        ctx.dispatcher.submit<SetControl>(inlet, sl->value());
      });
  QObject::connect(
      sl, &VSTGraphicsSlider::sliderReleased, context, [&ctx, sl]() {
        ctx.dispatcher.commit();
        sl->moving = false;
      });

  QObject::connect(
      &inlet, &vst3::ControlInlet::valueChanged, sl, [=](float val) {
        if (!sl->moving)
          sl->setValue(val);
      });

  QObject::connect(
      &inlet, &vst3::ControlInlet::executionValueChanged,
      sl, &VSTGraphicsSlider::setExecutionValue);

  QObject::connect(
      &inlet, &vst3::ControlInlet::executionReset, sl, &VSTGraphicsSlider::resetExecution);
  return sl;
}
}
