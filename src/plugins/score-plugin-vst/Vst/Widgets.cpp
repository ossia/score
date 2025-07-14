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
  char str[256]{};
  // Note that effGetParamDisplay will sometimes ignore the m_value argument
  // and just return the value for the actual parameter, which may be different
  // when executing...
  fx->dispatcher(fx, effGetParamDisplay, num, 0, str, m_value);

  m_execValue = fx->getParameter(fx, num);
  score::DefaultGraphicsSliderImpl::paint(
      *this, score::Skin::instance(), QString::fromUtf8(str), painter, widget);
}

EffectItem::EffectItem(
    const Model& effect, const Process::Context& doc, QGraphicsItem* root)
    : score::EmptyRectItem{root}
{
  rootItem = root;

  if(!effect.fx)
    return;
  if(!effect.fx->fx)
    return;

  QObject::connect(
      &effect, &Process::ProcessModel::controlAdded, this,
      [&](const Id<Process::Port>& id) {
    auto inlet = safe_cast<ControlInlet*>(effect.inlet(id));
    setupInlet(effect, *inlet, doc);
      });

  QObject::connect(
      &effect, &Process::ProcessModel::controlRemoved, this,
      [&](const Process::Port& port) {
    auto inlet = qobject_cast<const ControlInlet*>(&port);
    SCORE_ASSERT(inlet);
    auto it = ossia::find_if(controlItems, [&](auto p) { return p.first == inlet; });
    if(it != controlItems.end())
    {
      delete it->second;
      it = controlItems.erase(it);
      int i = std::distance(controlItems.begin(), it);
      for(; it != controlItems.end(); ++it, ++i)
      {
        score::EmptyRectItem* rect = it->second;
        QPointF pos = Process::currentWidgetPos(
            i, [&](int j) { return controlItems[j].second->boundingRect().size(); });

        rect->setPos(pos);
      }
    }
    updateRect();
      });

  const bool isSynth = effect.fx->fx->flags & effFlagsIsSynth;
  for(std::size_t i = VST_FIRST_CONTROL_INDEX(isSynth); i < effect.inlets().size(); i++)
  {
    auto inlet = safe_cast<ControlInlet*>(effect.inlets()[i]);
    setupInlet(effect, *inlet, doc);
  }
  updateRect();
}

void EffectItem::setupInlet(
    const Model& fx, ControlInlet& inlet, const Process::Context& doc)
{
  if(!fx.fx)
    return;

  int i = std::ssize(controlItems);

  auto csetup = Process::controlSetup(
      [](auto& factory, auto& inlet, const auto& doc, auto item, auto parent) {
    return factory.makePortItem(inlet, doc, item, parent);
      },
      [&](auto& factory, auto& inlet, const auto& doc, auto item, auto parent) {
    return VSTFloatSlider::make_item(fx.fx->fx, inlet, doc, item, parent);
  },
       [&](int j) { return controlItems[j].second->boundingRect().size(); },
       [&] { return inlet.name(); }, [](auto&&...) -> auto& {
         static ControlPortFactory f;
         return f;
       });

  // TODO useless, find a way to remove
  static const auto& portFactory
      = score::GUIAppContext().interfaces<Process::PortFactoryList>();
  auto ctl = Process::createControl(i, csetup, inlet, portFactory, doc, this, this);

  if(fx.fx->fx->numParams >= 10)
  {
    const auto& pixmaps = Process::Pixmaps::instance();
    auto rm_item = new score::QGraphicsPixmapButton{
        pixmaps.close_on, pixmaps.close_off, ctl.item};
    connect(
        rm_item, &score::QGraphicsPixmapButton::clicked, this,
        [&doc, &fx, id = inlet.id()] {
      QTimer::singleShot(0, [&doc, &fx, id] {
        CommandDispatcher<> disp{doc.commandStack};
        disp.submit<RemoveControl>(fx, id);
      });
        });

    rm_item->setPos(8., 16.);
  }
  controlItems.push_back({&inlet, ctl.item});
  updateRect();
}

void EffectItem::updateRect()
{
  QRectF cr = childrenBoundingRect();
  cr.setWidth(std::max(100., cr.width()));
  cr.setHeight(std::max(10., cr.height()));
  setRect(cr);
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
