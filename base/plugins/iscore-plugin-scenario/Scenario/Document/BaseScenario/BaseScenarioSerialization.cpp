
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>

#include "BaseScenario.hpp"
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
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

    readFrom(base_scenario.pluginModelList);

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(Scenario::BaseScenario& base_scenario)
{
    writeTo(static_cast<Scenario::BaseScenarioContainer&>(base_scenario));
    base_scenario.pluginModelList = iscore::ElementPluginModelList{*this, &base_scenario};

    checkDelimiter();
}


template<> void Visitor<Reader<JSONObject>>::readFrom(const Scenario::BaseScenario& base_scenario)
{
    readFrom(static_cast<const IdentifiedObject<Scenario::BaseScenario>&>(base_scenario));
    readFrom(static_cast<const Scenario::BaseScenarioContainer&>(base_scenario));

    m_obj["PluginsMetadata"] = toJsonValue(base_scenario.pluginModelList);
}


template<> void Visitor<Writer<JSONObject>>::writeTo(Scenario::BaseScenario& base_scenario)
{
    writeTo(static_cast<Scenario::BaseScenarioContainer&>(base_scenario));

    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    base_scenario.pluginModelList = iscore::ElementPluginModelList{elementPluginDeserializer, &base_scenario};
}
