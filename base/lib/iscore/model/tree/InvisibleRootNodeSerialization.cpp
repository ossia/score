#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

struct InvisibleRootNode;
template <typename T>
class Reader;
template <typename T>
class Writer;

template <>
ISCORE_LIB_BASE_EXPORT void
Visitor<Reader<DataStream>>::readFrom(const InvisibleRootNode&)
{
  insertDelimiter();
}

template <>
ISCORE_LIB_BASE_EXPORT void
Visitor<Writer<DataStream>>::writeTo(InvisibleRootNode&)
{
  checkDelimiter();
}

template <>
ISCORE_LIB_BASE_EXPORT void
Visitor<Reader<JSONObject>>::readFrom(const InvisibleRootNode&)
{
}

template <>
ISCORE_LIB_BASE_EXPORT void
Visitor<Writer<JSONObject>>::writeTo(InvisibleRootNode&)
{
}
