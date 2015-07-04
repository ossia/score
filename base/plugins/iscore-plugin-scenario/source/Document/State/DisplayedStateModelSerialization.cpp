#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "source/Document/State/DisplayedStateModel.hpp"

template<> void Visitor<Reader<DataStream>>::readFrom(const DisplayedStateModel& s)
{
    readFrom(static_cast<const IdentifiedObject<DisplayedStateModel>&>(s));

    readFrom(s.metadata);

    m_stream << s.m_eventId
             << s.m_previousConstraint
             << s.m_nextConstraint
             << s.m_heightPercentage

             << s.m_states;

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(DisplayedStateModel& s)
{
    writeTo(s.metadata);

    m_stream >> s.m_eventId
            >> s.m_previousConstraint
            >> s.m_nextConstraint
            >> s.m_heightPercentage

            >> s.m_states;

    checkDelimiter();
}

template<> void Visitor<Reader<JSONObject>>::readFrom(const DisplayedStateModel& s)
{
    readFrom(static_cast<const IdentifiedObject<DisplayedStateModel>&>(s));
    m_obj["Metadata"] = toJsonObject(s.metadata);

    m_obj["Event"] = toJsonValue(s.m_eventId);
    m_obj["PreviousConstraint"] = toJsonValue(s.m_previousConstraint);
    m_obj["NextConstraint"] = toJsonValue(s.m_nextConstraint);
    m_obj["HeightPercentage"] = s.m_heightPercentage;

    m_obj["States"] = toJsonArray(s.m_states);
}

template<> void Visitor<Writer<JSONObject>>::writeTo(DisplayedStateModel& s)
{
    s.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());

    s.m_eventId = fromJsonValue<id_type<EventModel>>(m_obj["Event"]);
    s.m_previousConstraint = fromJsonValue<id_type<ConstraintModel>>(m_obj["PreviousConstraint"]);
    s.m_nextConstraint = fromJsonValue<id_type<ConstraintModel>>(m_obj["NextConstraint"]);
    s.m_heightPercentage = m_obj["HeightPercentage"].toDouble();

    fromJsonArray(m_obj["States"].toArray(), s.m_states);
}
