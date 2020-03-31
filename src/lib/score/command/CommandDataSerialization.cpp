// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CommandData.hpp"

#include <score/plugins/StringFactoryKeySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

template <>
void DataStreamReader::read(const score::CommandData& d)
{
  m_stream << d.parentKey << d.commandKey << d.data;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(score::CommandData& d)
{
  m_stream >> d.parentKey >> d.commandKey >> d.data;
  checkDelimiter();
}
