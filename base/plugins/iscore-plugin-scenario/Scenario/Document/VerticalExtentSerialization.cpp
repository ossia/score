#include <QPoint>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>

#include "VerticalExtent.hpp"

template <>
void DataStreamReader::read(const Scenario::VerticalExtent& ve)
{
  m_stream << static_cast<QPointF>(ve);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Scenario::VerticalExtent& ve)
{
  m_stream >> static_cast<QPointF&>(ve);
  checkDelimiter();
}

template <>
void JSONValueReader::readFrom(const Scenario::VerticalExtent& ve)
{
  readFrom(static_cast<QPointF>(ve));
}

template <>
void JSONValueWriter::writeTo(Scenario::VerticalExtent& ve)
{
  writeTo(static_cast<QPointF&>(ve));
}
