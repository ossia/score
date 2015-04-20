#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "source/Document/Event/EventModel.hpp"

#include <State/State.hpp>

#include <API/Headers/Editor/TimeNode.h>


template<> void Visitor<Reader<DataStream>>::readFrom(const EventModel& ev)
{
    readFrom(static_cast<const IdentifiedObject<EventModel>&>(ev));

    m_stream << ev.metadata;

    m_stream << ev.previousConstraints()
             << ev.nextConstraints()
             << ev.heightPercentage();

    m_stream << ev.date();
    m_stream << ev.condition();
    m_stream << ev.timeNode();

    m_stream << ev.states();

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(EventModel& ev)
{
    m_stream >> ev.metadata;

    QVector<id_type<ConstraintModel>> prevCstr, nextCstr;
    double heightPercentage;
    TimeValue date;
    QString condition;
    id_type<TimeNodeModel> timenode;

    m_stream >> prevCstr
             >> nextCstr
             >> heightPercentage;

    m_stream >> date;
    m_stream >> condition;
    m_stream >> timenode;

    ev.m_previousConstraints = std::move(prevCstr);
    ev.m_nextConstraints = std::move(nextCstr);
    ev.setHeightPercentage(heightPercentage);
    ev.setDate(date);
    ev.setCondition(condition);
    ev.changeTimeNode(timenode);

    QList<State> states;
    m_stream >> states;
    ev.replaceStates(states);

    checkDelimiter();
}




template<> void Visitor<Reader<JSON>>::readFrom(const EventModel& ev)
{
    readFrom(static_cast<const IdentifiedObject<EventModel>&>(ev));
    m_obj["Metadata"] = toJsonObject(ev.metadata);

    m_obj["PreviousConstraints"] = toJsonArray(ev.previousConstraints());
    m_obj["NextConstraints"] = toJsonArray(ev.nextConstraints());
    m_obj["HeightPercentage"] = ev.heightPercentage();
    m_obj["Date"] = toJsonObject(ev.date());
    m_obj["Condition"] = ev.condition();
    m_obj["TimeNode"] = toJsonObject(ev.timeNode());

    m_obj["States"] = toJsonArray(ev.states());
}

template<> void Visitor<Writer<JSON>>::writeTo(EventModel& ev)
{
    ev.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());

    QVector<id_type<ConstraintModel>> prevCstr, nextCstr;

    fromJsonArray(m_obj["PreviousConstraints"].toArray(), prevCstr);
    fromJsonArray(m_obj["NextConstraints"].toArray(), nextCstr);
    ev.m_previousConstraints = std::move(prevCstr);
    ev.m_nextConstraints = std::move(nextCstr);

    ev.setHeightPercentage(m_obj["HeightPercentage"].toDouble());
    ev.setDate(fromJsonObject<TimeValue> (m_obj["Date"].toObject()));
    ev.setCondition(m_obj["Condition"].toString());
    ev.changeTimeNode(fromJsonObject<id_type<TimeNodeModel>> (m_obj["TimeNode"].toObject()));

    /* TODO JSON State deserialization
    QJsonArray states = m_obj["States"].toArray();

    for(auto json_vref : states)
    {
        Deserializer<JSON> deserializer {json_vref.toObject() };
        FakeState* state = new FakeState {deserializer, &ev};
        ev.addState(state);
    }
*/
}
