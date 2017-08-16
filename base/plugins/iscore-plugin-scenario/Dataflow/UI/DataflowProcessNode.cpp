#include "DataflowProcessNode.hpp"
#include <Dataflow/UI/NodeItem.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
namespace Process
{
DataflowProcessNode::~DataflowProcessNode()
{
  cleanup();
}

void Node::cleanup()
{
  auto& p = iscore::IDocument::documentContext(*this).model<Scenario::ScenarioDocumentModel>();
  for(const auto& cable : cables())
  {
    // TODO undo-redo fails with this
    p.cables.remove(cable);
  }
}
DataflowProcessNode::DataflowProcessNode(
    Process::DataflowProcess& proc)
  : Process::Node{Id<Process::Node>{}, &proc}
  , process{proc}
{
  connect(&process, &Process::DataflowProcess::inletsChanged,
          this, &DataflowProcessNode::inletsChanged);
  connect(&process, &Process::DataflowProcess::outletsChanged,
          this, &DataflowProcessNode::outletsChanged);

  auto& doc = iscore::IDocument::documentContext(proc);
  auto itm = new Dataflow::NodeItem{doc, *this};
  itm->setParentItem(doc.model<Scenario::ScenarioDocumentModel>().window.view.contentItem());
  itm->setPosition(QPointF(50, 50));
  ui = itm;
}
}
namespace Process
{

QString DataflowProcessNode::getText() const { return process.metadata().getName(); }

std::size_t DataflowProcessNode::audioInlets() const { return process.audioInlets(); }

std::size_t DataflowProcessNode::messageInlets() const { return process.messageInlets(); }

std::size_t DataflowProcessNode::midiInlets() const { return process.midiInlets(); }

std::size_t DataflowProcessNode::audioOutlets() const { return process.audioOutlets(); }

std::size_t DataflowProcessNode::messageOutlets() const{ return process.messageOutlets(); }

std::size_t DataflowProcessNode::midiOutlets() const{ return process.midiOutlets(); }

std::vector<Port> DataflowProcessNode::inlets() const { return process.inlets(); }

std::vector<Port> DataflowProcessNode::outlets() const { return process.outlets(); }

std::vector<Id<Cable> > DataflowProcessNode::cables() const { return process.cables; }

void DataflowProcessNode::addCable(Id<Cable> c) { process.cables.push_back(c); }

void DataflowProcessNode::removeCable(Id<Cable> c) {
  auto it = ossia::find(process.cables, c);
  if(it != process.cables.end())
    process.cables.erase(it);
}
bool operator==(const CableData& lhs, const CableData& rhs)
{
  return lhs.type == rhs.type
      && lhs.source == rhs.source
      && lhs.sink == rhs.sink
      && lhs.outlet == rhs.outlet
      && lhs.inlet == rhs.inlet;
}

DataflowProcess::DataflowProcess(
    TimeVal duration,
    const Id<ProcessModel>& id,
    const QString& name,
    QObject* parent):
  Process::ProcessModel{duration, id, name, parent}
{

}

DataflowProcess::DataflowProcess(
        const DataflowProcess& source,
        const Id<Process::ProcessModel>& id,
        const QString& name,
        QObject* parent):
  Process::ProcessModel{source, id, name, parent}
, m_inlets{source.m_inlets}
, m_outlets{source.m_outlets}
{
  updateCounts();
}

DataflowProcess::DataflowProcess(JSONObject::Deserializer& vis, QObject* parent):
  Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  updateCounts();
}

DataflowProcess::DataflowProcess(DataStream::Deserializer& vis, QObject* parent):
  Process::ProcessModel{vis, parent}
{
  vis.writeTo(*this);
  updateCounts();
}

DataflowProcess::~DataflowProcess()
{
  emit identified_object_destroying(this);
}

void DataflowProcess::setInlets(const std::vector<Port>& inlets)
{
  if(inlets != m_inlets)
  {
    m_inlets = inlets;
    updateCounts();
    emit inletsChanged();
  }
}

void DataflowProcess::setOutlets(const std::vector<Port>& outlets)
{
  if(outlets != m_outlets)
  {
    m_outlets = outlets;
    updateCounts();
    emit outletsChanged();
  }
}

const std::vector<Port>& DataflowProcess::inlets() const { return m_inlets; }

const std::vector<Port>& DataflowProcess::outlets() const { return m_outlets; }

void DataflowProcess::updateCounts()
{
  m_portCount = {};
  for(auto& p : m_inlets)
  {
    switch(p.type)
    {
      case PortType::Midi:
        m_portCount.midiIn++; break;
      case PortType::Audio:
        m_portCount.audioIn++; break;
      case PortType::Message:
        m_portCount.messageIn++; break;
    }
  }

  for(auto& p : m_outlets)
  {
    switch(p.type)
    {
      case PortType::Midi:
        m_portCount.midiOut++; break;
      case PortType::Audio:
        m_portCount.audioOut++; break;
      case PortType::Message:
        m_portCount.messageOut++; break;
    }
  }
}


}

template<>
ISCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Process::Port>(const Process::Port& p)
{
  m_stream << p.type << p.customData << p.address;
}
template<>
ISCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::Port>(Process::Port& p)
{
  m_stream >> p.type >> p.customData >> p.address;
}

template<>
ISCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Process::CableData>(const Process::CableData& p)
{
  m_stream << p.type << p.source << p.sink << p.outlet << p.inlet;
}
template<>
ISCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::CableData>(Process::CableData& p)
{
  m_stream >> p.type >> p.source >> p.sink >> p.outlet >> p.inlet;
}

template<>
ISCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read<Process::Port>(const Process::Port& p)
{
  obj["Type"] = (int)p.type;
  obj["Custom"] = p.customData;
  obj["Address"] = toJsonObject(p.address);
}
template<>
ISCORE_LIB_PROCESS_EXPORT void JSONObjectWriter::write<Process::Port>(Process::Port& p)
{
  p.type = (Process::PortType)obj["Type"].toInt();
  p.customData = obj["Custom"].toString();
  p.address = fromJsonObject<State::AddressAccessor>(obj["Address"]);
}

template<>
ISCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read<Process::CableData>(const Process::CableData& p)
{
  obj["Type"] = (int)p.type;
  obj["Source"] = toJsonObject(p.source);
  obj["Sink"] = toJsonObject(p.sink);
  obj["Outlet"] = toJsonValue(p.outlet);
  obj["Inlet"] = toJsonValue(p.inlet);
}
template<>
ISCORE_LIB_PROCESS_EXPORT void JSONObjectWriter::write<Process::CableData>(Process::CableData& p)
{
  p.type = (Process::CableType) obj["Type"].toInt();
  p.source = fromJsonObject<Path<Process::Node>>(obj["Source"]);
  p.sink = fromJsonObject<Path<Process::Node>>(obj["Sink"]);
  p.outlet = fromJsonValue<optional<int>>(obj["Oulet"]);
  p.inlet = fromJsonValue<optional<int>>(obj["Inlet"]);
}

template<>
ISCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Process::Cable>(const Process::Cable& p)
{
  m_stream << (const Process::CableData&)p;
}
template<>
ISCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::Cable>(Process::Cable& p)
{
  m_stream >> (Process::CableData&)p;
}
template<>
ISCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read<Process::Cable>(const Process::Cable& p)
{
  read((const Process::CableData&)p);
}
template<>
ISCORE_LIB_PROCESS_EXPORT void JSONObjectWriter::write<Process::Cable>(Process::Cable& p)
{
  write((Process::CableData&)p);
}

template<>
ISCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Process::DataflowProcess>(const Process::DataflowProcess& p)
{
  m_stream << p.cables << p.m_inlets << p.m_outlets;
}
template<>
ISCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::DataflowProcess>(Process::DataflowProcess& p)
{
  m_stream >> p.cables >> p.m_inlets >> p.m_outlets;
}

template<>
ISCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read<Process::DataflowProcess>(const Process::DataflowProcess& p)
{
  obj["Cables"] = toJsonValueArray(p.cables);
  obj["Inlets"] = toJsonArray(p.m_inlets);
  obj["Outlets"] = toJsonArray(p.m_outlets);
}
template<>
ISCORE_LIB_PROCESS_EXPORT void JSONObjectWriter::write<Process::DataflowProcess>(Process::DataflowProcess& p)
{
  fromJsonValueArray(obj["Cables"].toArray(), p.cables);
  fromJsonArray(obj["Inlets"].toArray(), p.m_inlets);
  fromJsonArray(obj["Outlets"].toArray(), p.m_outlets);
}
