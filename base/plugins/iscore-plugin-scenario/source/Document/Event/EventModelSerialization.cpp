#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "source/Document/Event/EventModel.hpp"


template<> void Visitor<Reader<DataStream>>::readFrom(const EventModel& ev)
{
    readFrom(static_cast<const IdentifiedObject<EventModel>&>(ev));

    readFrom(ev.metadata);

    m_stream << ev.previousConstraints()
             << ev.nextConstraints()
             << ev.heightPercentage();

    m_stream << ev.date();
    m_stream << ev.condition();
    m_stream << ev.trigger();
    m_stream << ev.timeNode();

    m_stream << ev.states();

    readFrom(ev.m_pluginModelList);

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(EventModel& ev)
{
    writeTo(ev.metadata);

    QVector<id_type<ConstraintModel>> prevCstr, nextCstr;
    double heightPercentage;
    TimeValue date;
    QString condition, trigger;
    id_type<TimeNodeModel> timenode;

    m_stream >> prevCstr
             >> nextCstr
             >> heightPercentage;

    m_stream >> date;
    m_stream >> condition >> trigger;
    m_stream >> timenode;

    ev.m_previousConstraints = std::move(prevCstr);
    ev.m_nextConstraints = std::move(nextCstr);
    ev.setHeightPercentage(heightPercentage);
    ev.setDate(date);
    ev.setCondition(condition);
    ev.setTrigger(trigger);
    ev.changeTimeNode(timenode);

    QList<iscore::State> states;
    m_stream >> states;
    ev.replaceStates(states);

    ev.m_pluginModelList = iscore::ElementPluginModelList{*this, &ev};

    checkDelimiter();
}




template<> void Visitor<Reader<JSONObject>>::readFrom(const EventModel& ev)
{
    readFrom(static_cast<const IdentifiedObject<EventModel>&>(ev));
    m_obj["Metadata"] = toJsonObject(ev.metadata);

    m_obj["PreviousConstraints"] = toJsonArray(ev.previousConstraints());
    m_obj["NextConstraints"] = toJsonArray(ev.nextConstraints());
    m_obj["HeightPercentage"] = ev.heightPercentage();
    m_obj["Date"] = toJsonValue(ev.date());
    m_obj["Condition"] = ev.condition();
    m_obj["Trigger"] = ev.trigger();
    m_obj["TimeNode"] = toJsonValue(ev.timeNode());

    m_obj["States"] = toJsonArray(ev.states());

    m_obj["PluginsMetadata"] = toJsonValue(ev.m_pluginModelList);
}

template<> void Visitor<Writer<JSONObject>>::writeTo(EventModel& ev)
{
    ev.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());

    fromJsonValueArray(m_obj["PreviousConstraints"].toArray(), ev.m_previousConstraints);
    fromJsonValueArray(m_obj["NextConstraints"].toArray(), ev.m_nextConstraints);

    ev.setHeightPercentage(m_obj["HeightPercentage"].toDouble());
    ev.setDate(fromJsonValue<TimeValue> (m_obj["Date"]));
    ev.setCondition(m_obj["Condition"].toString());
    ev.setTrigger(m_obj["Trigger"].toString());
    ev.changeTimeNode(fromJsonValue<id_type<TimeNodeModel>> (m_obj["TimeNode"]));

    QList<iscore::State> states;
    for(QJsonValue json_val : m_obj["States"].toArray())
    {
        states.push_back(fromJsonObject<iscore::State>(json_val.toObject()));
    }

    ev.replaceStates(states);

    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    ev.m_pluginModelList = iscore::ElementPluginModelList{elementPluginDeserializer, &ev};
}
