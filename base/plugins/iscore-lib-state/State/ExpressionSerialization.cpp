#include <QJsonObject>
#include <QJsonValue>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VariantSerialization.hpp>

#include "Expression.hpp"
#include "Relation.hpp"
#include <iscore/serialization/JSONValueVisitor.hpp>


template <>
void DataStreamReader::read(const State::Pulse& rel)
{
  m_stream << rel.address;

  insertDelimiter();
}


template <>
void JSONObjectReader::read(const State::Pulse& rel)
{
  obj[strings.address] = toJsonObject(rel.address);
}


template <>
void DataStreamWriter::writeTo(State::Pulse& rel)
{
  m_stream >> rel.address;

  checkDelimiter();
}


template <>
void JSONObjectWriter::writeTo(State::Pulse& rel)
{
  fromJsonObject(obj[strings.address], rel.address);
}


template <>
void DataStreamReader::read(const State::Relation& rel)
{
  m_stream << rel.lhs << rel.op << rel.rhs;

  insertDelimiter();
}


template <>
void JSONObjectReader::read(const State::Relation& rel)
{
  // TODO harmonize from... with marshall(..) in VisitorCommon.hpp
  obj[strings.LHS] = toJsonObject(rel.lhs);
  obj[strings.Op] = toJsonValue(rel.op);
  obj[strings.RHS] = toJsonObject(rel.rhs);
}


template <>
void DataStreamWriter::writeTo(State::Relation& rel)
{
  m_stream >> rel.lhs >> rel.op >> rel.rhs;

  checkDelimiter();
}


template <>
void JSONObjectWriter::writeTo(State::Relation& rel)
{
  fromJsonObject(obj[strings.LHS], rel.lhs);
  fromJsonValue(obj[strings.Op], rel.op);
  fromJsonObject(obj[strings.RHS], rel.rhs);
}


template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::ExprData& expr)
{
  readFrom(expr.m_data);
  insertDelimiter();
}


template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const State::ExprData& expr)
{
  readFrom(expr.m_data);
}


template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::writeTo(State::ExprData& expr)
{
  writeTo(expr.m_data);
  checkDelimiter();
}


template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectWriter::writeTo(State::ExprData& expr)
{
  writeTo(expr.m_data);
}
