
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>

#include "BaseScenario.hpp"
#include <iscore/serialization/JSONValueVisitor.hpp>

namespace Scenario
{
class BaseScenarioContainer;
}
template <typename T> class Reader;
template <typename T> class Writer;
template <typename model> class IdentifiedObject;

template<> void Visitor<Reader<DataStream>>::readFrom(const Scenario::BaseScenario& base_scenario)
{
    readFrom(static_cast<const IdentifiedObject<Scenario::BaseScenario>&>(base_scenario));
    readFrom(static_cast<const Scenario::BaseScenarioContainer&>(base_scenario));

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(Scenario::BaseScenario& base_scenario)
{
    writeTo(static_cast<Scenario::BaseScenarioContainer&>(base_scenario));

    checkDelimiter();
}


template<> void Visitor<Reader<JSONObject>>::readFrom(const Scenario::BaseScenario& base_scenario)
{
    readFrom(static_cast<const IdentifiedObject<Scenario::BaseScenario>&>(base_scenario));
    readFrom(static_cast<const Scenario::BaseScenarioContainer&>(base_scenario));
}


template<> void Visitor<Writer<JSONObject>>::writeTo(Scenario::BaseScenario& base_scenario)
{
    writeTo(static_cast<Scenario::BaseScenarioContainer&>(base_scenario));
}
