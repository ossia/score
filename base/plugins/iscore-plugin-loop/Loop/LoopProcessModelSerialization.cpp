#include <QJsonObject>
#include <QJsonValue>

#include "LoopProcessModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;


template <>
void DataStreamReader::read(const Loop::ProcessModel& proc)
{
  readFrom(static_cast<const Scenario::BaseScenarioContainer&>(proc));

  insertDelimiter();
}


template <>
void DataStreamWriter::writeTo(Loop::ProcessModel& proc)
{
  writeTo(static_cast<Scenario::BaseScenarioContainer&>(proc));

  checkDelimiter();
}


template <>
void JSONObjectReader::read(const Loop::ProcessModel& proc)
{
  readFrom(static_cast<const Scenario::BaseScenarioContainer&>(proc));
}


template <>
void JSONObjectWriter::writeTo(Loop::ProcessModel& proc)
{
  writeTo(static_cast<Scenario::BaseScenarioContainer&>(proc));
}
