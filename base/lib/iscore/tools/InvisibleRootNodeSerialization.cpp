#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

struct InvisibleRootNodeTag;
template <typename T> class Reader;
template <typename T> class Writer;

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<DataStream>>::readFrom(const InvisibleRootNodeTag&)
{
    insertDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<DataStream>>::writeTo(InvisibleRootNodeTag&)
{
    checkDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Reader<JSONObject>>::readFrom(const InvisibleRootNodeTag&)
{
}

template<>
ISCORE_LIB_BASE_EXPORT void Visitor<Writer<JSONObject>>::writeTo(InvisibleRootNodeTag&)
{
}

