#include "MessageNode.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const StateNodeValues& val)
{
    ISCORE_TODO;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(StateNodeValues& val)
{
    ISCORE_TODO;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const StateNodeValues& val)
{
    ISCORE_TODO;
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(StateNodeValues& val)
{
    ISCORE_TODO;
}


template<>
void Visitor<Reader<DataStream>>::readFrom(const StateNodeData& node)
{
    m_stream << node.name << node.values;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(StateNodeData& node)
{
    m_stream >> node.name >> node.values;
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const StateNodeData& node)
{
    m_obj["Name"] = node.name;
    readFrom(node.values);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(StateNodeData& node)
{
    node.name = m_obj["Name"].toString();
    writeTo(node.values);
}
