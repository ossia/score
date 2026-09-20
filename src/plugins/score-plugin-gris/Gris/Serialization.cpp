#include <Gris/Model.hpp>

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortSerialization.hpp>

#include <score/application/ApplicationComponents.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

template <>
void DataStreamReader::read(const Gris::SpatModel& proc)
{
  m_stream << proc.sourceCount();
  readPorts(*this, proc.m_inlets, proc.m_outlets);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gris::SpatModel& proc)
{
  int count{};
  m_stream >> count;
  proc.m_sourceCount = count;
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
  checkDelimiter();
}

template <>
void JSONReader::read(const Gris::SpatModel& proc)
{
  obj["SourceCount"] = proc.sourceCount();
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(Gris::SpatModel& proc)
{
  proc.m_sourceCount = obj["SourceCount"].toInt();
  writePorts(
      *this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets,
      proc.m_outlets, &proc);
}
