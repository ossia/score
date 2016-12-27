#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "JSProcessModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

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
void DataStreamWriter::writeTo(JS::ProcessModel& proc)
{
  QString str;
  m_stream >> str;
  proc.setScript(str);

  checkDelimiter();
}


template <>
void JSONObjectReader::readFromConcrete(const JS::ProcessModel& proc)
{
  obj["Script"] = proc.script();
}


template <>
void JSONObjectWriter::writeTo(JS::ProcessModel& proc)
{
  proc.setScript(obj["Script"].toString());
}
