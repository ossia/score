#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "Value.hpp"
#include <State/ValueConversion.hpp>
#include <iscore/tools/VariantSerialization.hpp>
using namespace iscore;
// TODO clean this file
template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::ValueImpl& value)
{
    readFrom(value.m_variant);
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::ValueImpl& value)
{
    writeTo(value.m_variant);
    checkDelimiter();
}


template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::no_value_t& value)
{
}


template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::no_value_t& value)
{
}

template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::impulse_t& value)
{
}


template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::impulse_t& value)
{
}

template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::tuple_t& value)
{
    readFrom_vector_obj_impl(*this, value);
    insertDelimiter();
}


template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::tuple_t& value)
{
    writeTo_vector_obj_impl(*this, value);
    checkDelimiter();
}



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
