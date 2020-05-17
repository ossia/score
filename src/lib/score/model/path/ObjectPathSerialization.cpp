// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ObjectPath.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
SCORE_LIB_BASE_EXPORT void DataStreamReader::read(const ObjectPath& path)
{
  readFrom(path.vec());
}

template <>
SCORE_LIB_BASE_EXPORT void DataStreamWriter::write(ObjectPath& path)
{
  writeTo(path.vec());
}

template <>
SCORE_LIB_BASE_EXPORT void JSONReader::read(const ObjectPath& path)
{
  readFrom(path.vec());
}

template <>
SCORE_LIB_BASE_EXPORT void JSONWriter::write(ObjectPath& path)
{
  writeTo(path.vec());
}
