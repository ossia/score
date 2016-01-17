#include <State/ValueConversion.hpp>

#include <boost/none_t.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/VariantSerialization.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include <QChar>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <iscore/tools/std/Optional.hpp>

#include "Value.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

// TODO clean this file
template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const State::ValueImpl& value)
{
    readFrom(value.m_variant);
    insertDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<DataStream>>::writeTo(State::ValueImpl& value)
{
    writeTo(value.m_variant);
    checkDelimiter();
}


template<>
void Visitor<Reader<DataStream>>::readFrom(const State::no_value_t& value)
{
}


template<>
void Visitor<Writer<DataStream>>::writeTo(State::no_value_t& value)
{
}

template<>
void Visitor<Reader<DataStream>>::readFrom(const State::impulse_t& value)
{
}


template<>
void Visitor<Writer<DataStream>>::writeTo(State::impulse_t& value)
{
}

template<>
void Visitor<Reader<DataStream>>::readFrom(const State::tuple_t& value)
{
    readFrom_vector_obj_impl(*this, value);
    insertDelimiter();
}


template<>
void Visitor<Writer<DataStream>>::writeTo(State::tuple_t& value)
{
    writeTo_vector_obj_impl(*this, value);
    checkDelimiter();
}



template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const State::Value& value)
{
    readFrom(value.val);
    insertDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<DataStream>>::writeTo(State::Value& value)
{
    writeTo(value.val);
    checkDelimiter();
}

ISCORE_LIB_STATE_EXPORT QJsonValue ValueToJson(const State::Value & value)
{
    return State::convert::value<QJsonValue>(value);
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const State::Value& val)
{
    m_obj["Type"] = State::convert::textualType(val);
    m_obj["Value"] = ValueToJson(val);
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(State::Value& val)
{
    val = State::convert::toValue(m_obj["Value"], m_obj["Type"].toString());
}

// TODO refactor with generic Optional Serialization somewhere else
template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const State::OptionalValue& obj)
{
    m_stream << static_cast<bool>(obj);

    if(obj)
    {
        m_stream << *obj;
    }
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<DataStream>>::writeTo(State::OptionalValue& obj)
{
    bool b {};
    m_stream >> b;

    if(b)
    {
        State::Value val;
        m_stream >> val;

        obj = val;
    }
    else
    {
        reset(obj);
    }
}
