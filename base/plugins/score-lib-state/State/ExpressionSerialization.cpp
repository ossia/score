// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonObject>
#include <QJsonValue>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VariantSerialization.hpp>

#include "Expression.hpp"
#include "Relation.hpp"
#include <score/serialization/JSONValueVisitor.hpp>
#include <State/ValueSerialization.hpp>


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
void DataStreamWriter::write(State::Pulse& rel)
{
  m_stream >> rel.address;

  checkDelimiter();
}


template <>
void JSONObjectWriter::write(State::Pulse& rel)
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
void DataStreamWriter::write(State::Relation& rel)
{
  m_stream >> rel.lhs >> rel.op >> rel.rhs;

  checkDelimiter();
}


template <>
void JSONObjectWriter::write(State::Relation& rel)
{
  fromJsonObject(obj[strings.LHS], rel.lhs);
  fromJsonValue(obj[strings.Op], rel.op);
  fromJsonObject(obj[strings.RHS], rel.rhs);
}


template <>
SCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::ExprData& expr)
{
  readFrom(expr.m_data);
  insertDelimiter();
}


template <>
SCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const State::ExprData& expr)
{
  readFrom(expr.m_data);
}


template <>
SCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(State::ExprData& expr)
{
  writeTo(expr.m_data);
  checkDelimiter();
}


template <>
SCORE_LIB_STATE_EXPORT void
JSONObjectWriter::write(State::ExprData& expr)
{
  writeTo(expr.m_data);
}
