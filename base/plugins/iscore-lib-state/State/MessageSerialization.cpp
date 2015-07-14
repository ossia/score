#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "Message.hpp"

using namespace iscore;
template<>
void Visitor<Reader<DataStream>>::readFrom(const Message& mess)
{
    readFrom(mess.address);
    m_stream << mess.value;
    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const Message& mess)
{
    m_obj["Address"] = toJsonObject(mess.address);
    m_obj["Value"] = mess.value.toString(); // TODO : NOPENOPENOPE. Use serialization in AddressSettings. Make a Value class.
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Message& mess)
{
    writeTo(mess.address);
    m_stream >> mess.value;

    checkDelimiter();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Message& mess)
{
    mess.address = fromJsonObject<Address>(m_obj["Address"].toObject());
    mess.value = QJsonValue(m_obj["Value"]).toVariant();
}
