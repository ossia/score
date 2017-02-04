#include <State/ValueConversion.hpp>
#include <State/ValueSerialization.hpp>

#include <QChar>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <boost/none_t.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/StringConstants.hpp>
#include <iscore/serialization/VariantSerialization.hpp>
#include <iscore/tools/std/Optional.hpp>

#include "Value.hpp"

// TODO clean this file

template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::ValueImpl& value)
{
  readFrom(value.m_variant);
  insertDelimiter();
}


template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(State::ValueImpl& value)
{
  writeTo(value.m_variant);
  checkDelimiter();
}


template <>
void DataStreamReader::read(const State::impulse& value)
{
}


template <>
void DataStreamWriter::write(State::impulse& value)
{
}


template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::Value& value)
{
  readFrom(value.val);
  insertDelimiter();
}


template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::write(State::Value& value)
{
  writeTo(value.val);
  checkDelimiter();
}

ISCORE_LIB_STATE_EXPORT QJsonValue ValueToJson(const State::Value& value)
{
  return State::convert::value<QJsonValue>(value);
}


template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectReader::read(const State::Value& val)
{
  obj[strings.Type] = State::convert::textualType(val);
  obj[strings.Value] = ValueToJson(val);
}


template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectWriter::write(State::Value& val)
{
  val = State::convert::fromQJsonValue(
      obj[strings.Value], obj[strings.Type].toString());
}
