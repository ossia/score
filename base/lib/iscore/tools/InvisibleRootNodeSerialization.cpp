#include "InvisibleRootNode.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const InvisibleRootNodeTag&)
{
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(InvisibleRootNodeTag&)
{
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const InvisibleRootNodeTag&)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(InvisibleRootNodeTag&)
{
}

