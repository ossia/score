#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

struct InvisibleRootNodeTag;
template <typename T> class Reader;
template <typename T> class Writer;

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

