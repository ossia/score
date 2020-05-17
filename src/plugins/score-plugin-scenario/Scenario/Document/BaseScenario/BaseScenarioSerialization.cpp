// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "BaseScenario.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>


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
void DataStreamReader::read(const Scenario::BaseScenario& base_scenario)
{
  readFrom(static_cast<const Scenario::BaseScenarioContainer&>(base_scenario));

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Scenario::BaseScenario& base_scenario)
{
  writeTo(static_cast<Scenario::BaseScenarioContainer&>(base_scenario));

  checkDelimiter();
}

template <>
void JSONReader::read(const Scenario::BaseScenario& base_scenario)
{
  readFrom(static_cast<const Scenario::BaseScenarioContainer&>(base_scenario));
}

template <>
void JSONWriter::write(Scenario::BaseScenario& base_scenario)
{
  writeTo(static_cast<Scenario::BaseScenarioContainer&>(base_scenario));
}
