#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "SettableIdentifier.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const boost::optional<int32_t>& obj)
{
    m_stream << bool (obj);

    if(obj)
    {
        m_stream << get(obj);
    }
}

template<>
void Visitor<Writer<DataStream>>::writeTo(boost::optional<int32_t>& obj)
{
    bool b {};
    m_stream >> b;

    if(b)
    {
        int32_t val;
        m_stream >> val;

        obj = val;
    }
    else
    {
        obj = boost::none_t {};
    }
}

template<>
void Visitor<Reader<JSON>>::readFrom(const boost::optional<int32_t>& obj)
{
    if(obj)
    {
        m_obj["id"] = get(obj);
    }
    else
    {
        m_obj["id"] = "None";
    }
}

template<>
void Visitor<Writer<JSON>>::writeTo(boost::optional<int32_t>& obj)
{
    if(m_obj["id"].toString() == "None")
    {
        obj = boost::none_t {};
    }
    else
    {
        obj = m_obj["id"].toInt();
    }
}
