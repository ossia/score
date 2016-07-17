#include <QJsonObject>
#include <QJsonValue>

#include "LoopProcessModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const Loop::ProcessModel& proc)
{
    readFrom(static_cast<const Scenario::BaseScenarioContainer&>(proc));

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Loop::ProcessModel& proc)
{
    writeTo(static_cast<Scenario::BaseScenarioContainer&>(proc));

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Loop::ProcessModel& proc)
{
    readFrom(static_cast<const Scenario::BaseScenarioContainer&>(proc));
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Loop::ProcessModel& proc)
{
    writeTo(static_cast<Scenario::BaseScenarioContainer&>(proc));
}
