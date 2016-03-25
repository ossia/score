#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/VariantSerialization.hpp>
#include <QJsonObject>
#include <QJsonValue>

#include "Expression.hpp"
#include "Relation.hpp"
#include <iscore/serialization/JSONValueVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const State::AddressAccessor& rel)
{
    m_stream << rel.address << rel.accessors;

    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const State::AddressAccessor& rel)
{
    m_obj[iscore::StringConstant().address] = toJsonObject(rel.address);
    m_obj["Accessors"] = toJsonValueArray(rel.accessors);
}

template<>
void Visitor<Writer<DataStream>>::writeTo(State::AddressAccessor& rel)
{
    m_stream >> rel.address >> rel.accessors;

    checkDelimiter();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(State::AddressAccessor& rel)
{
    fromJsonObject(m_obj[iscore::StringConstant().address], rel.address);
    fromJsonArray(m_obj["Accessors"].toArray(), rel.accessors);
}


template<>
void Visitor<Reader<DataStream>>::readFrom(const State::Pulse& rel)
{
    m_stream << rel.address;

    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const State::Pulse& rel)
{
    m_obj[iscore::StringConstant().address] = toJsonObject(rel.address);
}

template<>
void Visitor<Writer<DataStream>>::writeTo(State::Pulse& rel)
{
    m_stream >> rel.address;

    checkDelimiter();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(State::Pulse& rel)
{
    fromJsonObject(m_obj[iscore::StringConstant().address], rel.address);
}

template<>
void Visitor<Reader<DataStream>>::readFrom(const State::Relation& rel)
{
    m_stream << rel.lhs
             << rel.op
             << rel.rhs;

    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const State::Relation& rel)
{
    // TODO harmonize from... with marshall(..) in VisitorCommon.hpp
    m_obj[iscore::StringConstant().LHS] = toJsonObject(rel.lhs);
    m_obj[iscore::StringConstant().Op] = toJsonValue(rel.op);
    m_obj[iscore::StringConstant().RHS] = toJsonObject(rel.rhs);
}

template<>
void Visitor<Writer<DataStream>>::writeTo(State::Relation& rel)
{
    m_stream >> rel.lhs
             >> rel.op
             >> rel.rhs;

    checkDelimiter();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(State::Relation& rel)
{
    fromJsonObject(m_obj[iscore::StringConstant().LHS], rel.lhs);
    fromJsonValue(m_obj[iscore::StringConstant().Op], rel.op);
    fromJsonObject(m_obj[iscore::StringConstant().RHS], rel.rhs);
}


template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const ExprData& expr)
{
    readFrom(expr.m_data);
    insertDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const ExprData& expr)
{
    readFrom(expr.m_data);
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<DataStream>>::writeTo(ExprData& expr)
{
    writeTo(expr.m_data);
    checkDelimiter();
}

template<>
ISCORE_LIB_STATE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(ExprData& expr)
{
    writeTo(expr.m_data);
}


