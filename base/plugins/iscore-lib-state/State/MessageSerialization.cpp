#include <QDataStream>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QtGlobal>
#include <iscore/serialization/DataStreamVisitor.hpp>

#include "Message.hpp"
#include "ValueConversion.hpp"
#include "ValueSerialization.hpp"
#include <State/Address.hpp>
#include <State/Value.hpp>


template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamReader::read(const State::Message& mess)
{
  readFrom(mess.address);
  readFrom(mess.value);
  insertDelimiter();
}


template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectReader::readFrom(const State::Message& mess)
{
  obj[strings.Address] = toJsonObject(mess.address);
  obj[strings.Type] = State::convert::textualType(mess.value);
  obj[strings.Value] = ValueToJson(mess.value);
}


template <>
ISCORE_LIB_STATE_EXPORT void
DataStreamWriter::writeTo(State::Message& mess)
{
  writeTo(mess.address);
  writeTo(mess.value);

  checkDelimiter();
}

template <>
ISCORE_LIB_STATE_EXPORT void
JSONObjectWriter::writeTo(State::Message& mess)
{
  mess.address
      = fromJsonObject<State::AddressAccessor>(obj[strings.Address]);
  mess.value = State::convert::fromQJsonValue(
      obj[strings.Value], obj[strings.Type].toString());
}
