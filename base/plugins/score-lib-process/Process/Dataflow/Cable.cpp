#include "Cable.hpp"
#include <Process/Dataflow/Port.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <core/presenter/DocumentManager.hpp>
namespace Process
{

Cable::~Cable()
{

}

Cable::Cable(Id<Cable> c, QObject* parent)
  : IdentifiedObject{c, Metadata<ObjectKey_k, Process::Cable>::get(), parent}
{

}

Cable::Cable(Id<Cable> c, const CableData& data, QObject* parent):
  IdentifiedObject{c, Metadata<ObjectKey_k, Process::Cable>::get(), parent}
{
  m_type = data.type;
  m_source = data.source;
  m_sink = data.sink;
}

void Cable::update(const CableData& data)
{
  m_type = data.type;
  m_source = data.source;
  m_sink = data.sink;
}

CableData Cable::toCableData() const
{
  CableData c;
  c.type = m_type;
  c.source = m_source;
  c.sink = m_sink;

  return c;
}

CableType Cable::type() const
{
  return m_type;
}

Path<Process::Outlet> Cable::source() const
{
  return m_source;
}

Path<Process::Inlet> Cable::sink() const
{
  return m_sink;
}

void Cable::setType(CableType type)
{
  if (m_type == type)
    return;

  m_type = type;
  emit typeChanged(m_type);
}

}
template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Process::CableData>(const Process::CableData& p)
{
  m_stream << p.type << p.source << p.sink;
  insertDelimiter();
}
template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::CableData>(Process::CableData& p)
{
  m_stream >> p.type >> p.source >> p.sink;
  checkDelimiter();
}

template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read<Process::CableData>(const Process::CableData& p)
{
  obj["Type"] = (int)p.type;
  obj["Source"] = toJsonObject(p.source);
  obj["Sink"] = toJsonObject(p.sink);
}
template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectWriter::write<Process::CableData>(Process::CableData& p)
{
  p.type = (Process::CableType) obj["Type"].toInt();
  p.source = fromJsonObject<Path<Process::Outlet>>(obj["Source"]);
  p.sink = fromJsonObject<Path<Process::Inlet>>(obj["Sink"]);
}

template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Process::Cable>(const Process::Cable& p)
{
  m_stream << p.toCableData();
}
template<>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::Cable>(Process::Cable& p)
{
  Process::CableData cd;
  m_stream >> cd;
  p.update(cd);
}
template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read<Process::Cable>(const Process::Cable& p)
{
  obj["Data"] = toJsonObject(p.toCableData());
}
template<>
SCORE_LIB_PROCESS_EXPORT void JSONObjectWriter::write<Process::Cable>(Process::Cable& p)
{
  Process::CableData cd = fromJsonObject<Process::CableData>(obj["Data"].toObject());
  p.update(cd);
}

