#include "VerticalExtent.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const VerticalExtent& ve)
{
    m_stream << ve.top << ve.bottom;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(VerticalExtent& ve)
{
    m_stream >> ve.top >> ve.bottom;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONValue>>::readFrom(const VerticalExtent& ve)
{
    val = QJsonArray{ve.top, ve.bottom};
}

template<>
void Visitor<Writer<JSONValue>>::writeTo(VerticalExtent& ve)
{
    auto arr = val.toArray();
    ve.top = arr.at(0).toDouble();
    ve.bottom = arr.at(1).toDouble();
}
