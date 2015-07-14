#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "Message.hpp"

using namespace iscore;
template<>
void Visitor<Reader<DataStream>>::readFrom(const Message& mess)
{
    readFrom(mess.address);
    ISCORE_TODO; //readFrom(mess.value);
    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const Message& mess)
{
    m_obj["Address"] = toJsonObject(mess.address);
    ISCORE_TODO; //m_obj["Value"] = toJsonValue(mess.value); // TODO : NOPENOPENOPE. Use serialization in AddressSettings. Make a Value class.
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Message& mess)
{
    writeTo(mess.address);
    ISCORE_TODO; //writeTo(mess.value);

    checkDelimiter();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Message& mess)
{
    mess.address = fromJsonObject<Address>(m_obj["Address"].toObject());
    ISCORE_TODO; //mess.value = fromJsonValue<iscore::Value>(m_obj["Value"]);
}
