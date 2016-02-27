#include <iscore/serialization/DataStreamVisitor.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "Message.hpp"
#include <State/Address.hpp>
#include <State/Value.hpp>
#include "ValueConversion.hpp"
#include "ValueSerialization.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const State::Message& mess)
{
    readFrom(mess.address);
    readFrom(mess.value);
    insertDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const State::Message& mess)
{
    m_obj[iscore::StringConstant().Address] = toJsonObject(mess.address);
    m_obj[iscore::StringConstant().Type] = State::convert::textualType(mess.value);
    m_obj[iscore::StringConstant().Value] = ValueToJson(mess.value);
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<DataStream>>::writeTo(State::Message& mess)
{
    writeTo(mess.address);
    writeTo(mess.value);

    checkDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(State::Message& mess)
{
    mess.address = fromJsonObject<State::Address>(m_obj[iscore::StringConstant().Address].toObject());
    mess.value = State::convert::fromQJsonValue(
                m_obj[iscore::StringConstant().Value],
                m_obj[iscore::StringConstant().Type].toString());
}
