#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "source/Document/Event/EventModel.hpp"

#include <State/State.hpp>

#include <API/Headers/Editor/TimeNode.h>


template<> void Visitor<Reader<DataStream>>::readFrom(const EventModel& ev)
{
    readFrom(static_cast<const IdentifiedObject<EventModel>&>(ev));

    m_stream << ev.previousConstraints()
             << ev.nextConstraints()
             << ev.heightPercentage()
             << ev.constraintsYPos()
             << ev.bottomY()
             << ev.topY();

    m_stream << ev.date(); // should be in OSSIA API
    m_stream << ev.condition();
    m_stream << ev.timeNode();

    m_stream << ev.states();

    // TODO save OSSIA::TimeNode
    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(EventModel& ev)
{
    QVector<id_type<ConstraintModel>> prevCstr, nextCstr;
    QMap<id_type<ConstraintModel>, double> cstrYPos;
    double heightPercentage, bottomY, topY;
    TimeValue date;
    QString condition;
    id_type<TimeNodeModel> timenode;
    m_stream >> prevCstr
             >> nextCstr
             >> heightPercentage
             >> cstrYPos
             >> bottomY
             >> topY;

    m_stream >> date; // should be in OSSIA API
    m_stream >> condition;
    m_stream >> timenode;

    ev.m_previousConstraints = std::move(prevCstr);
    ev.m_nextConstraints = std::move(nextCstr);
    ev.setHeightPercentage(heightPercentage);
    ev.m_constraintsYPos = std::move(cstrYPos);
    ev.setBottomY(bottomY);
    ev.setTopY(topY);
    ev.setDate(date);
    ev.setCondition(condition);
    ev.changeTimeNode(timenode);

    QList<State> states;
    m_stream >> states;
    ev.replaceStates(states);

    ev.setOSSIATimeNode(new OSSIA::TimeNode);
    // TODO load the timenode

    checkDelimiter();
}




template<> void Visitor<Reader<JSON>>::readFrom(const EventModel& ev)
{
    readFrom(static_cast<const IdentifiedObject<EventModel>&>(ev));

    m_obj["PreviousConstraints"] = toJsonArray(ev.previousConstraints());
    m_obj["NextConstraints"] = toJsonArray(ev.nextConstraints());
    m_obj["HeightPercentage"] = ev.heightPercentage();
    m_obj["ConstraintsYPos"] = toJsonMap(ev.constraintsYPos());
    m_obj["BottomY"] = ev.bottomY();
    m_obj["TopY"] = ev.topY();
    m_obj["Date"] = toJsonObject(ev.date());   // should be in OSSIA API
    m_obj["Condition"] = ev.condition();
    m_obj["TimeNode"] = toJsonObject(ev.timeNode());

    m_obj["States"] = toJsonArray(ev.states());

    // TODO save OSSIA::TimeNode
}

template<> void Visitor<Writer<JSON>>::writeTo(EventModel& ev)
{
    QVector<id_type<ConstraintModel>> prevCstr, nextCstr;
    ;
    fromJsonArray(m_obj["PreviousConstraints"].toArray(), prevCstr);
    fromJsonArray(m_obj["NextConstraints"].toArray(), nextCstr);
    auto ymap = fromJsonMap<id_type<ConstraintModel>> (m_obj["ConstraintsYPos"].toArray());
    ev.m_previousConstraints = std::move(prevCstr);
    ev.m_nextConstraints = std::move(nextCstr);
    ev.m_constraintsYPos = std::move(ymap);

    ev.setHeightPercentage(m_obj["HeightPercentage"].toDouble());
    ev.setBottomY(m_obj["BottomY"].toInt());
    ev.setTopY(m_obj["TopY"].toInt());
    ev.setDate(fromJsonObject<TimeValue> (m_obj["Date"].toObject()));
    ev.setCondition(m_obj["Condition"].toString());
    ev.changeTimeNode(fromJsonObject<id_type<TimeNodeModel>> (m_obj["TimeNode"].toObject()));

    /* TODO
    QJsonArray states = m_obj["States"].toArray();

    for(auto json_vref : states)
    {
        Deserializer<JSON> deserializer {json_vref.toObject() };
        FakeState* state = new FakeState {deserializer, &ev};
        ev.addState(state);
    }
*/
    ev.setOSSIATimeNode(new OSSIA::TimeNode);
}
