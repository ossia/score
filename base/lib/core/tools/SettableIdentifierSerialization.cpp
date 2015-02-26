
#include "interface/serialization/DataStreamVisitor.hpp"
#include "interface/serialization/JSONVisitor.hpp"
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
    m_obj["IdentifierSet"] = bool (obj);

    if(obj)
    {
        m_obj["Identifier"] = get(obj);
    }
}

template<>
void Visitor<Writer<JSON>>::writeTo(boost::optional<int32_t>& obj)
{
    if(m_obj["IdentifierSet"].toBool())
    {
        obj = m_obj["Identifier"].toInt();
    }
    else
    {
        obj = boost::none_t {};
    }
}
