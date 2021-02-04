#include "PortItem.hpp"

#include <Automation/Commands/SetAutomationMax.hpp>
#include <Dataflow/Commands/CreateModulation.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Process/Commands/EditPort.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <State/MessageListSerialization.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/domain/domain.hpp>

#include <QGraphicsSceneDragDropEvent>
#include <QMenu>
#include <QMimeData>

namespace Dataflow
{
template <typename Vec>
bool intersection_empty(const Vec& v1, const Vec& v2)
{
  for (const auto& e1 : v1)
  {
    for (const auto& e2 : v2)
    {
      if (e1 == e2)
        return false;
    }
  }
  return true;
}

void onCreateCable(
    const score::DocumentContext& ctx,
    Dataflow::PortItem* p1,
    Dataflow::PortItem* p2)
{
  auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();
  CommandDispatcher<> disp{ctx.commandStack};

  auto& port1 = p1->port();
  auto& port2 = p2->port();
  if (port1.parent() == port2.parent())
    return;

  if (!intersection_empty(port1.cables(), port2.cables()))
    return;

  if (port1.type() != port2.type())
    return;

  const Process::Port* source{};
  const Process::Port* sink{};
  auto o1 = qobject_cast<const Process::Outlet*>(&port1);
  auto i2 = qobject_cast<const Process::Inlet*>(&port2);
  if (o1 && i2)
  {
    source = &port1;
    sink = &port2;
  }
  else
  {
    auto o2 = qobject_cast<const Process::Outlet*>(&port2);
    auto i1 = qobject_cast<const Process::Inlet*>(&port1);
    if (o2 && i1)
    {
      source = &port2;
      sink = &port1;
    }
    else
    {
      return;
    }
  }

  Process::CableData cd;
  cd.type = Process::CableType::ImmediateGlutton;
  disp.submit<Dataflow::CreateCable>(
        plug, getStrongId(plug.cables),
        Process::CableType::ImmediateGlutton,
        *source, *sink);
}

AutomatablePortItem::~AutomatablePortItem() { }

void AutomatablePortItem::setupMenu(QMenu& menu, const score::DocumentContext& ctx)
{
  auto act = menu.addAction(QObject::tr("Create automation"));
  QObject::connect(
      act,
      &QAction::triggered,
      this,
      [this, &ctx] { on_createAutomation(ctx); },
      Qt::QueuedConnection);
}

void AutomatablePortItem::on_createAutomation(const score::DocumentContext& ctx)
{
  const QObject* obj = &m_port;
  while (obj)
  {
    auto parent = obj->parent();
    if (auto cst = qobject_cast<Scenario::IntervalModel*>(parent))
    {
      RedoMacroCommandDispatcher<Dataflow::CreateModulation> macro{ctx.commandStack};
      on_createAutomation(
          *cst, [&](auto cmd) { macro.submit(cmd); }, ctx);
      macro.commit();
      return;
    }
    else
    {
      obj = parent;
    }
  }
}

bool AutomatablePortItem::on_createAutomation(
    Scenario::IntervalModel& cst,
    std::function<void(score::Command*)> macro,
    const score::DocumentContext& ctx)
{
  if (m_port.type() != Process::PortType::Message)
    return false;
  auto ctrl = qobject_cast<const Process::ControlInlet*>(&m_port);
  if (!ctrl)
    return false;

  // Important note: AddLayerInNewSlot will recompute all the ports
  // in the UI, deleting / recreating them, including that one. So
  // we have to make sur that by this point we aren't using this portitem's data anymore
  auto& inlet = m_port;

  auto make_cmd = new Scenario::Command::AddOnlyProcessToInterval{
      cst, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), QString{}, {}};
  macro(make_cmd);

  auto lay_cmd = new Scenario::Command::AddLayerInNewSlot{cst, make_cmd->processId()};
  macro(lay_cmd);

  auto dom = ctrl->domain();
  auto min = dom.get().convert_min<float>();
  auto max = dom.get().convert_max<float>();

  State::Unit unit = ctrl->address().qualifiers.get().unit;
  auto& autom = safe_cast<Automation::ProcessModel&>(cst.processes.at(make_cmd->processId()));
  macro(new Automation::SetUnit{autom, unit});
  macro(new Automation::SetMin{autom, min});
  macro(new Automation::SetMax{autom, max});

  auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();

  macro(new Dataflow::CreateCable{
          plug, getStrongId(plug.cables),
          Process::CableType::ImmediateGlutton,
          *autom.outlet,
          inlet});
  return true;
}

void AutomatablePortItem::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  prepareGeometryChange();
  m_diam = 8.;
  update();
  auto& mime = *event->mimeData();

  auto& ctx = score::IDocument::documentContext(m_port);
  if (mime.formats().contains(score::mime::port()))
  {
    if (clickedPort && this != clickedPort)
    {
      onCreateCable(ctx, clickedPort, this);
    }
  }
  clickedPort = nullptr;

  CommandDispatcher<> disp{ctx.commandStack};
  if (mime.formats().contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();
    if (ml.empty())
      return;
    auto& newAddr = ml[0].address;

    if (newAddr == m_port.address())
      return;

    if (newAddr.address.device.isEmpty())
      return;

    disp.submit(new Process::ChangePortAddress{m_port, std::move(newAddr)});
  }
  event->accept();
}

PortItem* AutomatablePortFactory::makeItem(
    Process::Inlet& port,
    const Process::Context& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  return new Dataflow::AutomatablePortItem{port, ctx, parent};
}

PortItem* AutomatablePortFactory::makeItem(
    Process::Outlet& port,
    const Process::Context& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  return new Dataflow::AutomatablePortItem{port, ctx, parent};
}

}
