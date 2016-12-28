#include <iscore/model/tree/InvisibleRootNode.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template<>
ISCORE_LIB_BASE_EXPORT void
DataStreamReader::read(const InvisibleRootNode&)
{
  insertDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void
DataStreamWriter::write(InvisibleRootNode&)
{
  checkDelimiter();
}

template<>
ISCORE_LIB_BASE_EXPORT void
JSONObjectReader::read(const InvisibleRootNode&)
{
}

template<>
ISCORE_LIB_BASE_EXPORT void
JSONObjectWriter::write(InvisibleRootNode&)
{
}
