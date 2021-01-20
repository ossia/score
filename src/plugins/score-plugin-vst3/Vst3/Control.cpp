#include <Vst3/Control.hpp>
#include <Vst3/Widgets.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Inspector/InspectorLayout.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/TextLabel.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/tools/SafeCast.hpp>

#include <QMenu>
#include <QToolButton>

#include <wobjectimpl.h>
W_OBJECT_IMPL(vst3::ControlInlet)
namespace vst3
{

void VSTControlPortItem::setupMenu(QMenu& menu, const score::DocumentContext& ctx)
{
  auto rm_act = menu.addAction(QObject::tr("Remove port"));
  connect(rm_act, &QAction::triggered, this, [this, &ctx] {
    QTimer::singleShot(0, [&ctx, parent = port().parent(), id = port().id()] {
      CommandDispatcher<> disp{ctx.commandStack};
     // TODO disp.submit<RemoveVSTControl>(*static_cast<VSTEffectModel*>(parent), id);
    });
  });

}

bool VSTControlPortItem::on_createAutomation(
    Scenario::IntervalModel& cst,
    std::function<void(score::Command*)> macro,
    const score::DocumentContext& ctx)
{
  auto make_cmd = new Scenario::Command::AddOnlyProcessToInterval{
      cst, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), {}, {}};
  macro(make_cmd);

  auto lay_cmd = new Scenario::Command::AddLayerInNewSlot{cst, make_cmd->processId()};
  macro(lay_cmd);

  auto& autom = safe_cast<Automation::ProcessModel&>(cst.processes.at(make_cmd->processId()));
  macro(new Automation::SetMin{autom, 0.});
  macro(new Automation::SetMax{autom, 1.});

  auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();

  macro(new Dataflow::CreateCable{
          plug, getStrongId(plug.cables),
          Process::CableType::ImmediateGlutton,
          *autom.outlet, port()});

  return true;
}

VSTControlPortFactory::~VSTControlPortFactory() { }

UuidKey<Process::Port> VSTControlPortFactory::concreteKey() const noexcept
{
  return Metadata<ConcreteKey_k, ControlInlet>::get();
}

Process::Port* VSTControlPortFactory::load(const VisitorVariant& vis, QObject* parent)
{
  return score::deserialize_dyn(vis, [&](auto&& deserializer) {
    return new ControlInlet{deserializer, parent};
  });
}

Dataflow::PortItem* VSTControlPortFactory::makeItem(
    Process::Inlet& port,
    const Process::Context& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  auto port_item = new VSTControlPortItem{port, ctx, parent};
  // Dataflow::setupSimpleInlet(port_item, port, ctx, parent, context);
  return port_item;
}

Dataflow::PortItem* VSTControlPortFactory::makeItem(
    Process::Outlet& port,
    const Process::Context& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  return nullptr;
}

static void setupVSTControl(
    const ControlInlet& inlet,
    QWidget* inlet_widget,
    const score::DocumentContext& ctx,
    Inspector::Layout& vlay,
    QWidget* parent)
{
  // TODO refactor with PortWidgetSetup::setupControl
  auto widg = new QWidget;
  auto advBtn = new QToolButton{widg};
  advBtn->setIcon(makeIcon(QStringLiteral(":/icons/port_message.png")));

  auto lab = new TextLabel{inlet.customData(), widg};
  auto hl = new score::MarginLess<QHBoxLayout>{widg};
  hl->addWidget(advBtn);
  hl->addWidget(lab);

  auto sw = new QWidget{parent};
  sw->setContentsMargins(0, 0, 0, 0);
  auto hl2 = new score::MarginLess<QHBoxLayout>{sw};
  hl2->addSpacing(30);
  auto lay = new Inspector::Layout{};
  Process::PortWidgetSetup::setupInLayout(inlet, ctx, *lay, sw);
  hl2->addLayout(lay);

  QObject::connect(advBtn, &QToolButton::clicked, sw, [=] { sw->setVisible(!sw->isVisible()); });
  sw->setVisible(false);

  vlay.addRow(widg, inlet_widget);
  vlay.addRow(sw);
}

void VSTControlPortFactory::setupInletInspector(
    const Process::Inlet& port,
    const score::DocumentContext& ctx,
    QWidget* parent,
    Inspector::Layout& lay,
    QObject* context)
{
  auto& inl = safe_cast<const ControlInlet&>(port);
  auto proc = safe_cast<Model*>(port.parent());
  auto widg = VSTFloatSlider::make_widget(proc->fx.controller, inl, ctx, parent, context);

  setupVSTControl(inl, widg, ctx, lay, parent);
}
}
