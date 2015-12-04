#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
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

using namespace iscore;
template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const Message& mess)
{
    readFrom(mess.address);
    readFrom(mess.value);
    insertDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const Message& mess)
{
    m_obj["Address"] = toJsonObject(mess.address);
    m_obj["Type"] = iscore::convert::textualType(mess.value);
    m_obj["Value"] = ValueToJson(mess.value);
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<DataStream>>::writeTo(Message& mess)
{
    writeTo(mess.address);
    writeTo(mess.value);

    checkDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(Message& mess)
{
    mess.address = fromJsonObject<Address>(m_obj["Address"].toObject());
    mess.value = iscore::convert::toValue(m_obj["Value"], m_obj["Type"].toString());
}



template<>
void Visitor<Reader<DataStream>>::readFrom(const MessageList& mess)
{
    m_stream << mess;
    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const MessageList& mess)
{
    m_obj["Data"] = toJsonArray(mess);
}

template<>
void Visitor<Writer<DataStream>>::writeTo(MessageList& mess)
{
    m_stream >> mess;
    checkDelimiter();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(MessageList& mess)
{
    MessageList t;
    fromJsonArray(m_obj["Data"].toArray(), t);
    mess = t;
}
