#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <QPoint>

#include "VerticalExtent.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(const VerticalExtent& ve)
{
    m_stream << static_cast<QPointF>(ve);
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(VerticalExtent& ve)
{
    m_stream >> static_cast<QPointF&>(ve);
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONValue>>::readFrom(const VerticalExtent& ve)
{
    readFrom(static_cast<QPointF>(ve));
}

template<>
void Visitor<Writer<JSONValue>>::writeTo(VerticalExtent& ve)
{
    writeTo(static_cast<QPointF&>(ve));
}
