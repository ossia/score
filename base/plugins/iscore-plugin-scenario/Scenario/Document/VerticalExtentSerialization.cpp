#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <QPoint>

#include "VerticalExtent.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(const Scenario::VerticalExtent& ve)
{
    m_stream << static_cast<QPointF>(ve);
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Scenario::VerticalExtent& ve)
{
    m_stream >> static_cast<QPointF&>(ve);
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONValue>>::readFrom(const Scenario::VerticalExtent& ve)
{
    readFrom(static_cast<QPointF>(ve));
}

template<>
void Visitor<Writer<JSONValue>>::writeTo(Scenario::VerticalExtent& ve)
{
    writeTo(static_cast<QPointF&>(ve));
}
