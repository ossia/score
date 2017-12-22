// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "JSProcessModel.hpp"
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const JS::ProcessModel& proc)
{
  readPorts(*this, proc.m_inlets, proc.m_outlets);

  m_stream << proc.m_script;

  insertDelimiter();
}


template <>
void DataStreamWriter::write(JS::ProcessModel& proc)
{
  writePorts(*this, components.interfaces<Process::PortFactoryList>(), proc.m_inlets, proc.m_outlets, &proc);

  QString str;
  m_stream >> str;
  proc.setScript(str);

  checkDelimiter();
}


template <>
void JSONObjectReader::read(const JS::ProcessModel& proc)
{
  readPorts(obj, proc.m_inlets, proc.m_outlets);
  obj["Script"] = proc.script();
}

template <>
void JSONObjectWriter::write(JS::ProcessModel& proc)
{
  writePorts(obj, components.interfaces<Process::PortFactoryList>(), proc.m_inlets, proc.m_outlets, &proc);
  proc.setScript(obj["Script"].toString());
}
