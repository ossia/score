#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include "DynamicState.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const DynamicState& mess)
{
    ISCORE_TODO;
    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const DynamicState& mess)
{
    ISCORE_TODO;
}

template<>
void Visitor<Writer<DataStream>>::writeTo(DynamicState& mess)
{
    ISCORE_TODO;
    checkDelimiter();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(DynamicState& mess)
{
    ISCORE_TODO;
}
