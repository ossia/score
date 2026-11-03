#include "ConnectionsList.hpp"

#include <Device/Node/DeviceNode.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/ModelMetadata.hpp>

#include <ossia/detail/small_vector.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node_functions.hpp>

#include <QCoreApplication>

namespace Dataflow
{
namespace
{
QString tr(const char* s)
{
  return QCoreApplication::translate("Dataflow::ConnectionsPanel", s);
}

const score::ModelMetadata* metadataOf(const QObject& obj)
{
  return obj.findChild<score::ModelMetadata*>({}, Qt::FindDirectChildrenOnly);
}

QString processName(const Process::ProcessModel* p)
{
  if(!p)
    return tr("Unknown");
  return p->metadata().getName();
}

//! The explorer's children of the root hold device settings, not address
//! settings: the generic child lookups do not apply at that level
const Device::Node* deviceNode(const Device::Node& root, const QString& name)
{
  for(const auto& n : root)
    if(n.is<Device::DeviceSettings>() && n.get<Device::DeviceSettings>().name == name)
      return &n;
  return nullptr;
}

ConnectionRow::Status addressStatus(
    const State::Address& addr, const Explorer::DeviceDocumentPlugin* devices)
{
  for(const auto& part : addr.path)
    for(QChar c : part)
      if(c == '*' || c == '?' || c == '[' || c == '{')
        return ConnectionRow::Pattern;

  if(!devices)
    return ConnectionRow::MissingDevice;

  // What the explorer shows first, then the live tree of devices it does not
  // show, such as the local and audio ones
  if(auto node = deviceNode(devices->rootNode(), addr.device))
  {
    if(addr.path.isEmpty() || Device::try_getNodeFromString(*node, addr.path))
      return ConnectionRow::Resolved;
  }

  auto dev = devices->list().findDevice(addr.device);
  if(!dev)
    return ConnectionRow::MissingDevice;
  if(addr.path.isEmpty())
    return ConnectionRow::Resolved;
  if(auto ossia_dev = dev->getDevice())
    if(ossia::net::find_node(
           ossia_dev->get_root_node(), QString("/" + addr.path.join(QChar('/'))).toStdString()))
      return ConnectionRow::Resolved;
  return ConnectionRow::MissingNode;
}
}

QString portTypeName(Process::PortType t)
{
  switch(t)
  {
    case Process::PortType::Message:
      return tr("Value");
    case Process::PortType::Audio:
      return tr("Audio");
    case Process::PortType::Midi:
      return tr("MIDI");
    case Process::PortType::Texture:
      return tr("Texture");
    case Process::PortType::Geometry:
      return tr("Geometry");
  }
  return {};
}

QString cableTypeName(int t)
{
  switch(Process::CableType(t))
  {
    case Process::CableType::ImmediateGlutton:
      return tr("Immediate Glutton");
    case Process::CableType::ImmediateStrict:
      return tr("Immediate Strict");
    case Process::CableType::DelayedGlutton:
      return tr("Delayed Glutton");
    case Process::CableType::DelayedStrict:
      return tr("Delayed Strict");
  }
  return {};
}

QString statusName(ConnectionRow::Status s)
{
  switch(s)
  {
    case ConnectionRow::Resolved:
      return tr("Bound");
    case ConnectionRow::MissingDevice:
      return tr("Missing device");
    case ConnectionRow::MissingNode:
      return tr("Missing address");
    case ConnectionRow::Pattern:
      return tr("Pattern");
  }
  return {};
}

QString describePort(const Process::Port& port)
{
  QString name = port.visualName();
  auto proc = Process::parentProcess(&port);
  if(proc)
  {
    // Unnamed ports, and ports sharing their name with another, are told apart
    // by their position in the process
    auto disambiguate = [&](const auto& ports, const QString& unnamed) {
      auto it = std::find(ports.begin(), ports.end(), &port);
      if(it == ports.end())
        return false;
      const int index = 1 + std::distance(ports.begin(), it);
      if(name.isEmpty())
        name = unnamed.arg(index);
      else if(std::count_if(ports.begin(), ports.end(), [&](auto p) {
                return p->visualName() == name;
              }) > 1)
        name = QStringLiteral("%1 %2").arg(name).arg(index);
      return true;
    };
    if(!disambiguate(proc->inlets(), tr("Inlet %1")))
      disambiguate(proc->outlets(), tr("Outlet %1"));
  }
  if(!proc)
    return name;
  return QStringLiteral("%1: %2").arg(processName(proc), name);
}

QString hierarchyPath(const QObject& obj)
{
  ossia::small_vector<QString, 8> names;
  for(auto p = obj.parent(); p; p = p->parent())
  {
    if(qobject_cast<const Scenario::ScenarioDocumentModel*>(p))
      break;
    if(qobject_cast<const Scenario::IntervalModel*>(p)
       || qobject_cast<const Process::ProcessModel*>(p))
      if(auto m = metadataOf(*p))
        names.push_back(m->getName());
  }
  QString res;
  for(auto it = names.rbegin(); it != names.rend(); ++it)
  {
    if(!res.isEmpty())
      res += QStringLiteral(" / ");
    res += *it;
  }
  return res;
}

std::vector<ConnectionRow> collectConnections(
    const Scenario::ScenarioDocumentModel& doc, const score::DocumentContext& ctx)
{
  std::vector<ConnectionRow> rows;
  rows.reserve(doc.cables.size());

  for(auto& cable : doc.cables)
  {
    auto source = cable.source().try_find(ctx);
    auto sink = cable.sink().try_find(ctx);
    if(!source || !sink)
      continue;

    ConnectionRow r;
    r.kind = ConnectionRow::Cable;
    r.cable = const_cast<Process::Cable*>(&cable);
    r.sourcePort = source;
    r.sinkPort = sink;
    r.sourceProcess = Process::parentProcess(source);
    r.sinkProcess = Process::parentProcess(sink);
    r.portType = source->type();
    r.from = describePort(*source);
    r.to = describePort(*sink);
    r.fromGroup = processName(r.sourceProcess);
    r.toGroup = processName(r.sinkProcess);
    r.mode = cableTypeName(int(cable.type()));

    const QString srcPath = r.sourceProcess ? hierarchyPath(*r.sourceProcess) : QString{};
    const QString sinkPath = r.sinkProcess ? hierarchyPath(*r.sinkProcess) : QString{};
    r.location = srcPath == sinkPath ? srcPath : tr("%1 to %2").arg(srcPath, sinkPath);
    rows.push_back(std::move(r));
  }

  auto devices = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  const auto processes = doc.findChildren<Process::ProcessModel*>();
  auto addPort = [&](Process::Port& port, Process::ProcessModel& proc, bool inlet) {
    const auto& addr = port.address();
    if(!addr.address.isSet())
      return;

    ConnectionRow r;
    r.kind = ConnectionRow::Address;
    r.addressIsSource = inlet;
    r.sourcePort = &port;
    r.sourceProcess = &proc;
    r.portType = port.type();
    r.address = addr.toString();
    r.device = addr.address.device;
    r.status = addressStatus(addr.address, devices);
    r.mode = statusName(r.status);
    r.location = hierarchyPath(proc);
    const QString portName = describePort(port);
    if(inlet)
    {
      r.from = r.address;
      r.to = portName;
      r.fromGroup = r.device;
      r.toGroup = proc.metadata().getName();
    }
    else
    {
      r.from = portName;
      r.to = r.address;
      r.fromGroup = proc.metadata().getName();
      r.toGroup = r.device;
    }
    rows.push_back(std::move(r));
  };

  for(auto proc : processes)
  {
    for(auto inlet : proc->inlets())
      addPort(*inlet, *proc, true);
    for(auto outlet : proc->outlets())
      addPort(*outlet, *proc, false);
  }
  return rows;
}
}
