#include "WiimoteSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>


template <>
void DataStreamReader::read(const Protocols::WiimoteSpecificSettings& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::WiimoteSpecificSettings& n)
{
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::WiimoteSpecificSettings& n)
{
}

template <>
void JSONWriter::write(Protocols::WiimoteSpecificSettings& n)
{
}
