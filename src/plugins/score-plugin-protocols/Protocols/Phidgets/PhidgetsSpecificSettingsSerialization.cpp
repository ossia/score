// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_PHIDGETS)
#include "PhidgetsSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Protocols::PhidgetSpecificSettings& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::PhidgetSpecificSettings& n)
{
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::PhidgetSpecificSettings& n)
{
}

template <>
void JSONWriter::write(Protocols::PhidgetSpecificSettings& n)
{
}
#endif
