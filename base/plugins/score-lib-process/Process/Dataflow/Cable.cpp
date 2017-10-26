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

Cable::Cable(Id<Cable> c, QObject* parent): IdentifiedObject{c, "Cable", parent} { }

Cable::Cable(const score::DocumentContext& ctx, Id<Cable> c, const CableData& data, QObject* parent):
  IdentifiedObject{c, "Cable", parent}
{
  m_type = data.type;
  m_source = data.source.try_find(ctx);
  m_sink = data.sink.try_find(ctx);
}

void Cable::update(const score::DocumentContext& ctx, const CableData& data)
{
  source_node.reset();
  sink_node.reset();
  exec.reset();

  setType(data.type);
  setSource(data.source.try_find(ctx));
  setSink(data.sink.try_find(ctx));
}

CableData Cable::toCableData() const
{
  CableData c;
  c.type = m_type;
  if(m_source) c.source = *m_source;
  if(m_sink) c.sink = *m_sink;

  return c;
}

CableType Cable::type() const
{
  return m_type;
}

Process::Port* Cable::source() const
{
  return m_source;
}

Process::Port* Cable::sink() const
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

void Cable::setSource(Process::Port* source)
{
  if (m_source == source)
    return;
  if(m_source)
  {
    QObject::disconnect(m_srcDeath);
    m_source->removeCable(id());
  }

  m_source = source;

  if(m_source)
  {
    m_srcDeath = connect(m_source, &QObject::destroyed, this, [=] {
      setSource(nullptr);
    });
    m_source->addCable(id());
  }

  emit sourceChanged(m_source);
}

void Cable::setSink(Process::Port* sink)
{
  if (m_sink == sink)
    return;
  if(m_sink)
  {
    QObject::disconnect(m_sinkDeath);
    m_sink->removeCable(id());
  }

  m_sink = sink;

  if(m_sink)
  {
    m_sinkDeath = connect(m_sink, &QObject::destroyed, this, [=] {
      setSink(nullptr);
    });

    m_sink->addCable(id());
  }

  emit sinkChanged(m_sink);
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
  p.source = fromJsonObject<Path<Process::Port>>(obj["Source"]);
  p.sink = fromJsonObject<Path<Process::Port>>(obj["Sink"]);
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
  p.update(score::AppContext().documents.currentDocument()->context(), cd);
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
  p.update(score::AppContext().documents.currentDocument()->context(), cd);
}

