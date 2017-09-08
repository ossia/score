// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "JSProcessModel.hpp"
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;


template <>
void DataStreamReader::read(const JS::ProcessModel& proc)
{
  m_stream << proc.m_script;

  insertDelimiter();
}


template <>
void DataStreamWriter::write(JS::ProcessModel& proc)
{
  QString str;
  m_stream >> str;
  proc.setScript(str);

  checkDelimiter();
}


template <>
void JSONObjectReader::read(const JS::ProcessModel& proc)
{
  obj["Script"] = proc.script();
}


template <>
void JSONObjectWriter::write(JS::ProcessModel& proc)
{
  proc.setScript(obj["Script"].toString());
}
