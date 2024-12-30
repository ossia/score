// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BitfocusSpecificSettings.hpp"

#include <Protocols/NetworkWidgets/Serialization.hpp>

#include <score/serialization/BoostVariant2Serialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Protocols::BitfocusSpecificSettings& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::BitfocusSpecificSettings& n)
{
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::BitfocusSpecificSettings& n)
{
}

template <>
void JSONWriter::write(Protocols::BitfocusSpecificSettings& n)
{
}
