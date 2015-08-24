#include "InvisibleRootNode.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const InvisibleRootNodeTag& n)
{
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(InvisibleRootNodeTag& n)
{
    checkDelimiter();
}

// Move me
template<>
void Visitor<Reader<JSONObject>>::readFrom(const InvisibleRootNodeTag& n)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(InvisibleRootNodeTag& n)
{
}

