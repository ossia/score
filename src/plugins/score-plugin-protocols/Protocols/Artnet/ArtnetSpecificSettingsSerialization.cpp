#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include "ArtnetSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>


template <>
void DataStreamReader::read(const Protocols::ArtnetSpecificSettings& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::ArtnetSpecificSettings& n)
{
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::ArtnetSpecificSettings& n)
{
}

template <>
void JSONWriter::write(Protocols::ArtnetSpecificSettings& n)
{
}
#endif
