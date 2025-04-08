#include "PortItem.hpp"

#include <State/MessageListSerialization.hpp>

#include <Device/Node/NodeListMimeSerialization.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Process/ProcessContext.hpp>

#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>

#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>

#include <Automation/Commands/SetAutomationMax.hpp>
#include <Dataflow/Commands/CreateModulation.hpp>
#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>

#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/domain/domain.hpp>

#include <QApplication>
#include <QClipboard>
#include <QGraphicsSceneDragDropEvent>
#include <QMenu>
#include <QMimeData>

namespace Dataflow
{
template <typename Vec>
static bool intersection_empty(const Vec& v1, const Vec& v2)
{
  for(const auto& e1 : v1)
  {
    for(const auto& e2 : v2)
    {
      if(e1 == e2)
        return false;
    }
  }
  return true;
}

void onCreateCable(
    const score::DocumentContext& ctx, const Process::Port& port1,
    const Process::Port& port2)
{
  auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();
  CommandDispatcher<> disp{ctx.commandStack};

  if(port1.parent() == port2.parent())
    return;

  if(!intersection_empty(port1.cables(), port2.cables()))
    return;

  if(port1.type() != port2.type())
    return;

  const Process::Port* source{};
  const Process::Port* sink{};
  auto o1 = qobject_cast<const Process::Outlet*>(&port1);
  auto i2 = qobject_cast<const Process::Inlet*>(&port2);
  if(o1 && i2)
  {
    source = &port1;
    sink = &port2;
  }
  else
  {
    auto o2 = qobject_cast<const Process::Outlet*>(&port2);
    auto i1 = qobject_cast<const Process::Inlet*>(&port1);
    if(o2 && i1)
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
      plug, getStrongId(plug.cables), Process::CableType::ImmediateGlutton, *source,
      *sink);
}

void onCreateCable(
    const score::DocumentContext& ctx, Dataflow::PortItem* p1, Dataflow::PortItem* p2)
{
  onCreateCable(ctx, p1->port(), p2->port());
}

AutomatablePortItem::~AutomatablePortItem() { }

void AutomatablePortItem::setupMenu(QMenu& menu, const score::DocumentContext& ctx)
{
  if(!this->m_inlet)
    return;
  if(this->port().type() != Process::PortType::Message)
    return;
  {
    auto act = menu.addAction(QObject::tr("Create automation"));
    QObject::connect(act, &QAction::triggered, this, [this, &ctx] {
      on_createAutomation(ctx);
    }, Qt::QueuedConnection);
  }

  if(auto proc = qobject_cast<Process::ProcessModel*>(this->port().parent()))
  {
    if(auto addr = Process::processLocalTreeAddress(*proc); !addr.isEmpty())
    {
      addr += "/";
      addr += this->port().exposed();
      addr += "/value";
      auto act = menu.addAction(QObject::tr("Copy address"));
      QObject::connect(act, &QAction::triggered, &port(), [addr] {
        auto& cb = *qApp->clipboard();
        cb.setText(addr);
      }, Qt::QueuedConnection);
    }
  }
}

void AutomatablePortItem::on_createAutomation(const score::DocumentContext& ctx)
{
  const QObject* obj = &m_port;
  while(obj)
  {
    auto parent = obj->parent();
    if(auto cst = qobject_cast<Scenario::IntervalModel*>(parent))
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
    const Scenario::IntervalModel& cst, std::function<void(score::Command*)> macro,
    const score::DocumentContext& ctx)
{
  if(m_port.type() != Process::PortType::Message)
    return false;
  auto ctrl = qobject_cast<const Process::ControlInlet*>(&m_port);
  if(!ctrl)
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
  auto val = ossia::convert<float>(ctrl->value());
  if(ossia::safe_isnan(min) || ossia::safe_isinf(min))
    return false;
  if(ossia::safe_isnan(max) || ossia::safe_isinf(max))
    return false;
  if(ossia::safe_isnan(val) || ossia::safe_isinf(val))
    return false;

  if(min >= max)
    max = min + 1;

  auto segt = Curve::flatCurveSegment(val, min, max);

  State::Unit unit = ctrl->address().qualifiers.get().unit;
  auto& autom
      = safe_cast<Automation::ProcessModel&>(cst.processes.at(make_cmd->processId()));
  macro(new Automation::SetUnit{autom, unit});
  macro(new Automation::SetMin{autom, min});
  macro(new Automation::SetMax{autom, max});
  macro(new Curve::UpdateCurve{autom.curve(), {std::move(segt)}});

  auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();

  macro(new Dataflow::CreateCable{
      plug, getStrongId(plug.cables), Process::CableType::ImmediateGlutton,
      *autom.outlet, inlet});
  return true;
}

void AutomatablePortItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if(this->contains(event->pos()))
  {
    switch(event->button())
    {
      case Qt::RightButton:
        contextMenuRequested(event->scenePos(), event->screenPos());
        break;
      case Qt::LeftButton:
        if(qApp->keyboardModifiers() & Qt::CTRL)
        {
          auto sel = this->m_context.selectionStack.currentSelection();
          if(sel.size() == 1)
          {
            if(auto item = qobject_cast<Process::Port*>(sel.at(0).data()))
            {
              if(item != &this->m_port)
              {
                onCreateCable(this->m_context, *item, this->m_port);
              }
            }
          }
        }
      default:
        break;
    }
  }
  event->accept();
}
void AutomatablePortItem::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  prepareGeometryChange();
  m_diam = 8.;
  update();
  auto& mime = *event->mimeData();

  auto& ctx = score::IDocument::documentContext(m_port);
  if(mime.formats().contains(score::mime::port()))
  {
    if(clickedPort && this != clickedPort)
    {
      onCreateCable(ctx, clickedPort, this);
    }
  }
  clickedPort = nullptr;

  CommandDispatcher<> disp{ctx.commandStack};

  if(mime.formats().contains(score::mime::nodelist()))
  {
    Mime<Device::FreeNodeList>::Deserializer des{mime};
    Device::FreeNodeList nl = des.deserialize();

    if(nl.empty())
      return;

    if(nl[0].first == m_port.address().address)
      return;

    if(nl[0].first.device.isEmpty())
      return;

    auto addr = nl[0].second.target<Device::AddressSettings>();
    if(!addr)
      return;
    disp.submit(new Process::ChangePortSettings{
        m_port, {State::AddressAccessor{nl[0].first}, std::move(*addr)}});
  }
  else if(mime.formats().contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    State::MessageList ml = des.deserialize();
    if(ml.empty())
      return;
    auto& newAddr = ml[0].address;

    if(newAddr == m_port.address())
      return;

    if(newAddr.address.device.isEmpty())
      return;

    disp.submit(new Process::ChangePortAddress{m_port, std::move(newAddr)});
  }
  event->accept();
}

PortItem* AutomatablePortFactory::makePortItem(
    Process::Inlet& port, const Process::Context& ctx, QGraphicsItem* parent,
    QObject* context)
{
  return new Dataflow::AutomatablePortItem{port, ctx, parent};
}

PortItem* AutomatablePortFactory::makePortItem(
    Process::Outlet& port, const Process::Context& ctx, QGraphicsItem* parent,
    QObject* context)
{
  return new Dataflow::AutomatablePortItem{port, ctx, parent};
}

}
