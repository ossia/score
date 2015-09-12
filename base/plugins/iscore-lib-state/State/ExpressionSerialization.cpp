#include "Expression.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "Relation.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::Relation& rel)
{
    m_stream << rel.lhs
             << rel.op
             << rel.rhs;

    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const iscore::Relation& rel)
{
    // TODO harmonize from... with marshall(..) in VisitorCommon.hpp
    m_obj["LHS"] = toJsonObject(rel.lhs);
    m_obj["Op"] = toJsonValue(rel.op);
    m_obj["RHS"] = toJsonObject(rel.rhs);
}

template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::Relation& rel)
{
    m_stream >> rel.lhs
             >> rel.op
             >> rel.rhs;

    checkDelimiter();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(iscore::Relation& rel)
{
    fromJsonObject(m_obj["LHS"].toObject(), rel.lhs);
    fromJsonObject(m_obj["RHS"].toObject(), rel.rhs);
    fromJsonValue(m_obj["Op"], rel.op);
}

template<> class TypeToName<iscore::Address>
{ public: static constexpr const char * name() { return "Address"; } };

template<> class TypeToName<iscore::Value>
{ public: static constexpr const char * name() { return "Value"; } };

template<> class TypeToName<iscore::Relation>
{ public: static constexpr const char * name() { return "Relation"; } };

template<> class TypeToName<iscore::BinaryOperator>
{ public: static constexpr const char * name() { return "BinOp"; } };

template<> class TypeToName<iscore::UnaryOperator>
{ public: static constexpr const char * name() { return "UnOp"; } };


template<>
void Visitor<Reader<DataStream>>::readFrom(const ExprData& expr)
{
    readFrom(expr.m_data);
    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const ExprData& expr)
{
    readFrom(expr.m_data);
}

template<>
void Visitor<Writer<DataStream>>::writeTo(ExprData& expr)
{
    writeTo(expr.m_data);
    checkDelimiter();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(ExprData& expr)
{
    writeTo(expr.m_data);
}

template<>
void Visitor<Reader<DataStream>>::readFrom(const TreeNode<ExprData>& n)
{
    readFrom(static_cast<const ExprData&>(n));

    m_stream << n.childCount();
    for(const auto& child : n)
    {
        readFrom(child);
    }

    insertDelimiter();
}


template<>
void Visitor<Writer<DataStream>>::writeTo(TreeNode<ExprData>& n)
{
    writeTo(static_cast<ExprData&>(n));

    int childCount;
    m_stream >> childCount;
    for (int i = 0; i < childCount; ++i)
    {
        TreeNode<ExprData>* child = new TreeNode<ExprData>;
        writeTo(*child);
        n.insertChild(n.childCount(), child);
    }

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const TreeNode<ExprData>& n)
{
    readFrom(static_cast<const ExprData&>(n));
    m_obj["Children"] = toJsonArray(n);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(TreeNode<ExprData>& n)
{
    writeTo(static_cast<ExprData&>(n));
    for (const auto& val : m_obj["Children"].toArray())
    {
        TreeNode<ExprData>* child = new TreeNode<ExprData>;
        Deserializer<JSONObject> nodeWriter(val.toObject());

        nodeWriter.writeTo(*child);
        n.insertChild(n.childCount(), child);
    }
}

