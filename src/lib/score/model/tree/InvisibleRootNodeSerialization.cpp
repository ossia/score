// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score/model/tree/InvisibleRootNode.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
SCORE_LIB_BASE_EXPORT void DataStreamReader::read(const InvisibleRootNode&)
{
  insertDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamWriter::write(InvisibleRootNode&)
{
  checkDelimiter();
}

template <>
SCORE_LIB_BASE_EXPORT void JSONObjectReader::read(const InvisibleRootNode&)
{
}

template <>
SCORE_LIB_BASE_EXPORT void JSONObjectWriter::write(InvisibleRootNode&)
{
}
