#include <QJsonObject>
#include <QJsonValue>

#include "DummyModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Dummy::DummyModel& proc)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Dummy::DummyModel& proc)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Dummy::DummyModel& proc)
{
}

template <>
void JSONObjectWriter::write(Dummy::DummyModel& proc)
{
}
