#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Media/Commands/VSTCommands.hpp>
#include <Media/Effect/VST/VSTControl.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <QMenu>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::VST::VSTControlInlet)
namespace Media::VST
{

void VSTControlPortItem::setupMenu(
    QMenu& menu,
    const score::DocumentContext& ctx)
{
  auto rm_act = menu.addAction(QObject::tr("Remove port"));
  connect(rm_act, &QAction::triggered, this, [this, &ctx] {
    QTimer::singleShot(0, [&ctx, parent = port().parent(), id = port().id()] {
      CommandDispatcher<> disp{ctx.commandStack};
      disp.submit<RemoveVSTControl>(*static_cast<VSTEffectModel*>(parent), id);
    });
  });
}

bool VSTControlPortItem::on_createAutomation(
    Scenario::IntervalModel& cst,
    std::function<void(score::Command*)> macro,
    const score::DocumentContext& ctx)
{
  auto make_cmd = new Scenario::Command::AddOnlyProcessToInterval{
      cst, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), {}};
  macro(make_cmd);

  auto lay_cmd
      = new Scenario::Command::AddLayerInNewSlot{cst, make_cmd->processId()};
  macro(lay_cmd);

  auto& autom = safe_cast<Automation::ProcessModel&>(
      cst.processes.at(make_cmd->processId()));
  macro(new Automation::SetMin{autom, 0.});
  macro(new Automation::SetMax{autom, 1.});

  auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();
  Process::CableData cd;
  cd.type = Process::CableType::ImmediateGlutton;
  cd.source = *autom.outlet;
  cd.sink = port();

  macro(new Dataflow::CreateCable{
      plug, getStrongId(plug.cables), std::move(cd)});
  return true;
}

VSTControlPortFactory::~VSTControlPortFactory() {}

UuidKey<Process::Port> VSTControlPortFactory::concreteKey() const noexcept
{
  return Metadata<ConcreteKey_k, VSTControlInlet>::get();
}

Process::Port*
VSTControlPortFactory::load(const VisitorVariant& vis, QObject* parent)
{
  return score::deserialize_dyn(vis, [&](auto&& deserializer) {
    return new VSTControlInlet{deserializer, parent};
  });
}

Dataflow::PortItem* VSTControlPortFactory::makeItem(
    Process::Inlet& port,
    const score::DocumentContext& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  auto port_item = new VSTControlPortItem{port, ctx, parent};
  // Dataflow::setupSimpleInlet(port_item, port, ctx, parent, context);
  return port_item;
}

Dataflow::PortItem* VSTControlPortFactory::makeItem(
    Process::Outlet& port,
    const score::DocumentContext& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  return nullptr;
}
}
