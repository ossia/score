
#include <QJsonObject>
#include <QJsonValue>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "BaseScenario.hpp"
#include <iscore/serialization/JSONValueVisitor.hpp>

namespace Scenario
{
class BaseScenarioContainer;
}
template <typename T>
class Reader;
template <typename T>
class Writer;
template <typename model>
class IdentifiedObject;


template <>
void DataStreamReader::read(
    const Scenario::BaseScenario& base_scenario)
{
  readFrom(static_cast<const Scenario::BaseScenarioContainer&>(base_scenario));

  insertDelimiter();
}


template <>
void DataStreamWriter::write(
    Scenario::BaseScenario& base_scenario)
{
  writeTo(static_cast<Scenario::BaseScenarioContainer&>(base_scenario));

  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Scenario::BaseScenario& base_scenario)
{
  readFrom(static_cast<const Scenario::BaseScenarioContainer&>(base_scenario));
}


template <>
void JSONObjectWriter::write(
    Scenario::BaseScenario& base_scenario)
{
  writeTo(static_cast<Scenario::BaseScenarioContainer&>(base_scenario));
}
