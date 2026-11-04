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

#include <ossia/detail/hash_map.hpp>
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

namespace
{
//! Labels of the ports of a process, in one pass over each port list: named
//! as their node shows them; ports sharing their name with another are told
//! apart by their position in the process.
using PortLabels = ossia::hash_map<const Process::Port*, QString>;
PortLabels portLabels(const Process::ProcessModel& proc)
{
  PortLabels res;
  auto label = [&](const auto& ports) {
    ossia::hash_map<QString, int> counts;
    for(auto p : ports)
      counts[Process::displayName(*p)]++;
    int index = 1;
    for(auto p : ports)
    {
      QString name = Process::displayName(*p);
      if(counts[name] > 1)
        name = QStringLiteral("%1 %2").arg(name).arg(index);
      res.emplace(p, std::move(name));
      index++;
    }
  };
  label(proc.inlets());
  label(proc.outlets());
  return res;
}

//! The process or interval a port belongs to
const QObject* portOwner(const Process::Port& port)
{
  if(auto proc = Process::parentProcess(&port))
    return proc;
  for(auto p = port.parent(); p; p = p->parent())
    if(qobject_cast<const Scenario::IntervalModel*>(p))
      return p;
  return nullptr;
}

QString ownerName(const QObject* owner)
{
  if(!owner)
    return tr("Unknown");
  if(auto m = metadataOf(*owner))
    return m->getName();
  return tr("Unknown");
}

QString describePort(const Process::Port& port, const PortLabels* labels)
{
  QString name;
  if(labels)
    if(auto it = labels->find(&port); it != labels->end())
      name = it->second;
  if(name.isEmpty())
    name = Process::displayName(port);
  auto owner = portOwner(port);
  if(!owner)
    return name;
  return QStringLiteral("%1: %2").arg(ownerName(owner), name);
}
}

QString describePort(const Process::Port& port)
{
  if(auto proc = Process::parentProcess(&port))
  {
    const auto labels = portLabels(*proc);
    return describePort(port, &labels);
  }
  return describePort(port, nullptr);
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

  // Labels are computed once per process
  ossia::hash_map<const Process::ProcessModel*, PortLabels> labels;
  auto describe = [&](const Process::Port& port) {
    auto proc = Process::parentProcess(&port);
    if(!proc)
      return describePort(port, nullptr);
    auto it = labels.find(proc);
    if(it == labels.end())
      it = labels.emplace(proc, portLabels(*proc)).first;
    return describePort(port, &it->second);
  };

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
    r.sourceOwner = const_cast<QObject*>(portOwner(*source));
    r.sinkOwner = const_cast<QObject*>(portOwner(*sink));
    r.portType = source->type();
    r.from = describe(*source);
    r.to = describe(*sink);
    r.fromGroup = ownerName(r.sourceOwner);
    r.toGroup = ownerName(r.sinkOwner);
    r.mode = cableTypeName(int(cable.type()));

    const QString srcPath = r.sourceOwner ? hierarchyPath(*r.sourceOwner) : QString{};
    const QString sinkPath = r.sinkOwner ? hierarchyPath(*r.sinkOwner) : QString{};
    r.location = srcPath == sinkPath ? srcPath : tr("%1 to %2").arg(srcPath, sinkPath);
    rows.push_back(std::move(r));
  }

  auto devices = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  auto addPort = [&](Process::Port& port, QObject& owner, bool inlet) {
    const auto& addr = port.address();
    if(!addr.address.isSet())
      return;

    ConnectionRow r;
    r.kind = ConnectionRow::Address;
    r.addressIsSource = inlet;
    r.sourcePort = &port;
    r.sourceProcess = qobject_cast<Process::ProcessModel*>(&owner);
    r.sourceOwner = &owner;
    r.portType = port.type();
    r.address = addr.toString();
    r.device = addr.address.device;
    r.status = addressStatus(addr.address, devices);
    r.mode = statusName(r.status);
    r.location = hierarchyPath(owner);
    const QString portName = describe(port);
    if(inlet)
    {
      r.from = r.address;
      r.to = portName;
      r.fromGroup = r.device;
      r.toGroup = ownerName(&owner);
    }
    else
    {
      r.from = portName;
      r.to = r.address;
      r.fromGroup = ownerName(&owner);
      r.toGroup = r.device;
    }
    rows.push_back(std::move(r));
  };

  for(auto proc : doc.findChildren<Process::ProcessModel*>())
  {
    for(auto inlet : proc->inlets())
      addPort(*inlet, *proc, true);
    for(auto outlet : proc->outlets())
      addPort(*outlet, *proc, false);
  }
  for(auto itv : doc.findChildren<Scenario::IntervalModel*>())
  {
    if(itv->inlet)
      addPort(*itv->inlet, *itv, true);
    if(itv->outlet)
      addPort(*itv->outlet, *itv, false);
  }
  return rows;
}
}
