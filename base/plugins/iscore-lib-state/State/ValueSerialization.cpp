#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "Value.hpp"
#include <State/ValueConversion.hpp>

using namespace iscore;
template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::Value& value)
{
    m_stream << value.val;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::Value& value)
{
    m_stream >> value.val;
    checkDelimiter();
}

QJsonValue ValueToJson(const iscore::Value & value)
{
    return iscore::convert::value<QJsonValue>(value);
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const iscore::Value& val)
{
    m_obj["Type"] = iscore::convert::textualType(val);
    m_obj["Value"] = ValueToJson(val);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(iscore::Value& val)
{
    val = iscore::convert::toValue(m_obj["Value"], m_obj["Type"].toString());
}

// TODO refactor with generic Optional Serialization somewhere else
template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::OptionalValue& obj)
{
    m_stream << static_cast<bool>(obj);

    if(obj)
    {
        m_stream << *obj;
    }
}

template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::OptionalValue& obj)
{
    bool b {};
    m_stream >> b;

    if(b)
    {
        iscore::Value val;
        m_stream >> val;

        obj = val;
    }
    else
    {
        obj = boost::none_t {};
    }
}
