// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "JSProcessModel.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>

#include <score/application/ApplicationComponents.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QString>

template <>
void DataStreamReader::read(const JS::ProcessModel& proc)
{
  m_stream << proc.m_script;

  readPorts(*this, proc.m_inlets, proc.m_outlets);

  insertDelimiter();
}

template <>
void DataStreamWriter::write(JS::ProcessModel& proc)
{
  QString str;
  m_stream >> str;
  proc.setScript(str);

  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);

  checkDelimiter();
}

template <>
void JSONReader::read(const JS::ProcessModel& proc)
{
  obj["Script"] = proc.script();
  readPorts(*this, proc.m_inlets, proc.m_outlets);
}

template <>
void JSONWriter::write(JS::ProcessModel& proc)
{
  proc.setScript(obj["Script"].toString());
  writePorts(
      *this,
      components.interfaces<Process::PortFactoryList>(),
      proc.m_inlets,
      proc.m_outlets,
      &proc);
}
