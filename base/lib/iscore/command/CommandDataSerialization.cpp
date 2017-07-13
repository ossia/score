// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QByteArray>
#include <iscore/serialization/DataStreamVisitor.hpp>

#include "CommandData.hpp"
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;

template <>
void DataStreamReader::read(const iscore::CommandData& d)
{
  m_stream << d.parentKey << d.commandKey << d.data;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(iscore::CommandData& d)
{
  m_stream >> d.parentKey >> d.commandKey >> d.data;
  checkDelimiter();
}
