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
void JSONValueReader::read(const Scenario::VerticalExtent& ve)
{
  read(static_cast<QPointF>(ve));
}

template <>
void JSONValueWriter::write(Scenario::VerticalExtent& ve)
{
  write(static_cast<QPointF&>(ve));
}
