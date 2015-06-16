#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "Address.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const Address& a)
{
    m_stream << a.device << a.path;
    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const Address& a)
{
    m_obj["Device"] = a.device;
    m_obj["Path"] = a.path.join("/");
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Address& a)
{
    m_stream >> a.device >> a.path;
    checkDelimiter();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Address& a)
{
    a.device = m_obj["Device"].toString();

    a.path = m_obj["Path"].toString().split("/");
}
