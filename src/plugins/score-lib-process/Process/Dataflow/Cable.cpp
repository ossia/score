#include "Cable.hpp"

#include <Process/Dataflow/Port.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/JSONValueVisitor.hpp>

#include <core/presenter/DocumentManager.hpp>

#include <ossia/dataflow/graph_node.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Process::Cable)
namespace Process
{

Cable::~Cable() { }

Cable::Cable(Id<Cable> c, QObject* parent)
    : IdentifiedObject{c, Metadata<ObjectKey_k, Process::Cable>::get(), parent}
    , m_type{CableType::ImmediateGlutton}
{
}

Cable::Cable(Id<Cable> c, const CableData& data, QObject* parent)
    : IdentifiedObject{c, Metadata<ObjectKey_k, Process::Cable>::get(), parent}
{
  m_type = data.type;
  m_source = data.source;
  m_sink = data.sink;
  m_source.unsafePath().resetCache();
  m_sink.unsafePath().resetCache();
}

void Cable::update(const CableData& data)
{
  m_type = data.type;
  m_source = data.source;
  m_sink = data.sink;
  m_source.unsafePath().resetCache();
  m_sink.unsafePath().resetCache();
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
  typeChanged(m_type);
}

void Cable::resetCache() const noexcept
{
  IdentifiedObject::resetCache();
  m_source.unsafePath().resetCache();
  m_sink.unsafePath().resetCache();
}
}
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read<Process::CableData>(const Process::CableData& p)
{
  m_stream << p.type << p.source << p.sink;
  insertDelimiter();
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::CableData>(Process::CableData& p)
{
  m_stream >> p.type >> p.source >> p.sink;
  checkDelimiter();
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read<Process::CableData>(const Process::CableData& p)
{
  stream.StartObject();
  obj["Type"] = (int)p.type;
  obj["Source"] = p.source;
  obj["Sink"] = p.sink;
  stream.EndObject();
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write<Process::CableData>(Process::CableData& p)
{
  p.type = (Process::CableType)obj["Type"].toInt();
  p.source <<= obj["Source"];
  p.sink <<= obj["Sink"];
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read<Process::Cable>(const Process::Cable& p)
{
  m_stream << p.toCableData();
}
template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write<Process::Cable>(Process::Cable& p)
{
  Process::CableData cd;
  m_stream >> cd;
  p.update(cd);
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read<Process::Cable>(const Process::Cable& p)
{
  obj["Type"] = (int)p.m_type;
  obj["Source"] = p.m_source;
  obj["Sink"] = p.m_sink;
}
template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write<Process::Cable>(Process::Cable& p)
{
  p.m_type = (Process::CableType)obj["Type"].toInt();
  p.m_source <<= obj["Source"];
  p.m_sink <<= obj["Sink"];
  p.m_source.unsafePath().resetCache();
  p.m_sink.unsafePath().resetCache();
}
