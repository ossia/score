#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "source/Document/State/DisplayedStateModel.hpp"

template<> void Visitor<Reader<DataStream>>::readFrom(const DisplayedStateModel& s)
{
    readFrom(static_cast<const IdentifiedObject<DisplayedStateModel>&>(s));

    readFrom(s.metadata);

    m_stream << s.heightPercentage();

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(DisplayedStateModel& s)
{
    writeTo(s.metadata);

    double h;

    m_stream >> h;

    s.setHeightPercentage(h);

    checkDelimiter();
}

template<> void Visitor<Reader<JSONObject>>::readFrom(const DisplayedStateModel& s)
{
    readFrom(static_cast<const IdentifiedObject<DisplayedStateModel>&>(s));
    m_obj["Metadata"] = toJsonObject(s.metadata);
    m_obj["HeightPercentage"] = s.heightPercentage();
}

template<> void Visitor<Writer<JSONObject>>::writeTo(DisplayedStateModel& s)
{
    s.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());
    s.setHeightPercentage(m_obj["HeightPercentage"].toDouble());
}
