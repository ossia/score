// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "JSProcessModel.hpp"
#include <Process/Dataflow/Port.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const JS::ProcessModel& proc)
{
  m_stream << (int32_t)proc.m_inlets.size();
  for(auto v : proc.m_inlets)
    m_stream << *v;
  m_stream << (int32_t)proc.m_outlets.size();
  for(auto v : proc.m_outlets)
    m_stream << *v;

  m_stream << proc.m_script;

  insertDelimiter();
}


template <>
void DataStreamWriter::write(JS::ProcessModel& proc)
{
  {
    int32_t ports;
    m_stream >> ports;
    for(auto i = ports; i-->0;) {
      proc.m_inlets.push_back(new Process::Inlet{*this, &proc});
    }
  }
  {
    int32_t ports;
    m_stream >> ports;
    for(auto i = ports; i-->0;) {
      proc.m_outlets.push_back(new Process::Outlet{*this, &proc});
    }
  }

  QString str;
  m_stream >> str;
  proc.setScript(str);

  checkDelimiter();
}


template <>
void JSONObjectReader::read(const JS::ProcessModel& proc)
{
  obj["Inlets"] = toJsonArray(proc.m_inlets);
  obj["Outlets"] = toJsonArray(proc.m_outlets);
  obj["Script"] = proc.script();
}

template <>
void JSONObjectWriter::write(JS::ProcessModel& proc)
{
  fromJsonArray(obj["Inlets"].toArray(), proc.m_inlets, &proc);
  fromJsonArray(obj["Outlets"].toArray(), proc.m_outlets, &proc);
  proc.setScript(obj["Script"].toString());
}
