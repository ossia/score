// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Expression.hpp"
#include "Relation.hpp"

#include <State/ValueSerialization.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VariantSerialization.hpp>


template <>
void DataStreamReader::read(const State::Pulse& rel)
{
  m_stream << rel.address;

  insertDelimiter();
}

template <>
void JSONReader::read(const State::Pulse& rel)
{
  read(rel.address);
}

template <>
void DataStreamWriter::write(State::Pulse& rel)
{
  m_stream >> rel.address;

  checkDelimiter();
}

template <>
void JSONWriter::write(State::Pulse& rel)
{
  write(rel.address);
}

template <>
void DataStreamReader::read(const State::Relation& rel)
{
  m_stream << rel.lhs << rel.op << rel.rhs;

  insertDelimiter();
}

template <>
void JSONReader::read(const State::Relation& rel)
{
  stream.StartObject();
  // TODO harmonize from... with marshall(..) in VisitorCommon.hpp
  obj[strings.LHS] = rel.lhs;
  obj[strings.Op] = rel.op;
  obj[strings.RHS] = rel.rhs;
  stream.EndObject();
}

template <>
void DataStreamWriter::write(State::Relation& rel)
{
  m_stream >> rel.lhs >> rel.op >> rel.rhs;

  checkDelimiter();
}

template <>
void JSONWriter::write(State::Relation& rel)
{
  rel.lhs <<= obj[strings.LHS];
  rel.op <<= obj[strings.Op];
  rel.rhs <<= obj[strings.RHS];
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const State::ExprData& expr)
{
  readFrom(expr.impl());
  insertDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONReader::read(const State::ExprData& expr)
{
  obj["Expression"] = expr.impl();
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(State::ExprData& expr)
{
  writeTo(expr.impl());
  checkDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONWriter::write(State::ExprData& expr)
{
  expr.impl() <<= obj["Expression"];
}
