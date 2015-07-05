#include "VerticalExtent.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const VerticalExtent& ve)
{
    m_stream << ve.point;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(VerticalExtent& ve)
{
    m_stream >> ve.point;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONValue>>::readFrom(const VerticalExtent& ve)
{
    readFrom(ve.point);
}

template<>
void Visitor<Writer<JSONValue>>::writeTo(VerticalExtent& ve)
{
    writeTo(ve.point);
}
