// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "HTTPSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Protocols::HTTPSpecificSettings& n)
{
  m_stream << n.text;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::HTTPSpecificSettings& n)
{
  m_stream >> n.text;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::HTTPSpecificSettings& n)
{
  obj["Text"] = n.text;
}

template <>
void JSONWriter::write(Protocols::HTTPSpecificSettings& n)
{
  n.text = obj["Text"].toString();
}
