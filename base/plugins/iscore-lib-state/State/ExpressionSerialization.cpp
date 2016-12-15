#include <QJsonObject>
#include <QJsonValue>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VariantSerialization.hpp>

#include "Expression.hpp"
#include "Relation.hpp"
#include <iscore/serialization/JSONValueVisitor.hpp>

template <>
void Visitor<Reader<DataStream>>::readFrom(const State::Pulse& rel)
{
  m_stream << rel.address;

  insertDelimiter();
}

template <>
void Visitor<Reader<JSONObject>>::readFrom(const State::Pulse& rel)
{
  m_obj[strings.address] = toJsonObject(rel.address);
}

template <>
void Visitor<Writer<DataStream>>::writeTo(State::Pulse& rel)
{
  m_stream >> rel.address;

  checkDelimiter();
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(State::Pulse& rel)
{
  fromJsonObject(m_obj[strings.address], rel.address);
}

template <>
void Visitor<Reader<DataStream>>::readFrom(const State::Relation& rel)
{
  m_stream << rel.lhs << rel.op << rel.rhs;

  insertDelimiter();
}

template <>
void Visitor<Reader<JSONObject>>::readFrom(const State::Relation& rel)
{
  // TODO harmonize from... with marshall(..) in VisitorCommon.hpp
  m_obj[strings.LHS] = toJsonObject(rel.lhs);
  m_obj[strings.Op] = toJsonValue(rel.op);
  m_obj[strings.RHS] = toJsonObject(rel.rhs);
}

template <>
void Visitor<Writer<DataStream>>::writeTo(State::Relation& rel)
{
  m_stream >> rel.lhs >> rel.op >> rel.rhs;

  checkDelimiter();
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(State::Relation& rel)
{
  fromJsonObject(m_obj[strings.LHS], rel.lhs);
  fromJsonValue(m_obj[strings.Op], rel.op);
  fromJsonObject(m_obj[strings.RHS], rel.rhs);
}

template <>
ISCORE_LIB_STATE_EXPORT void
Visitor<Reader<DataStream>>::readFrom(const State::ExprData& expr)
{
  readFrom(expr.m_data);
  insertDelimiter();
}

template <>
ISCORE_LIB_STATE_EXPORT void
Visitor<Reader<JSONObject>>::readFrom(const State::ExprData& expr)
{
  readFrom(expr.m_data);
}

template <>
ISCORE_LIB_STATE_EXPORT void
Visitor<Writer<DataStream>>::writeTo(State::ExprData& expr)
{
  writeTo(expr.m_data);
  checkDelimiter();
}

template <>
ISCORE_LIB_STATE_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(State::ExprData& expr)
{
  writeTo(expr.m_data);
}
