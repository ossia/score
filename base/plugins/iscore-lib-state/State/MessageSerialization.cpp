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

template <typename T>
class Reader;
template <typename T>
class Writer;

template <>
ISCORE_LIB_STATE_EXPORT void
Visitor<Reader<DataStream>>::read(const State::Message& mess)
{
  readFrom(mess.address);
  readFrom(mess.value);
  insertDelimiter();
}

template <>
ISCORE_LIB_STATE_EXPORT void
Visitor<Reader<JSONObject>>::readFrom(const State::Message& mess)
{
  m_obj[strings.Address] = toJsonObject(mess.address);
  m_obj[strings.Type] = State::convert::textualType(mess.value);
  m_obj[strings.Value] = ValueToJson(mess.value);
}

template <>
ISCORE_LIB_STATE_EXPORT void
Visitor<Writer<DataStream>>::writeTo(State::Message& mess)
{
  writeTo(mess.address);
  writeTo(mess.value);

  checkDelimiter();
}

template <>
ISCORE_LIB_STATE_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(State::Message& mess)
{
  mess.address
      = fromJsonObject<State::AddressAccessor>(m_obj[strings.Address]);
  mess.value = State::convert::fromQJsonValue(
      m_obj[strings.Value], m_obj[strings.Type].toString());
}
