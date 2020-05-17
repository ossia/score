// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "VerticalExtent.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>

#include <QPoint>
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
/*
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
*/
