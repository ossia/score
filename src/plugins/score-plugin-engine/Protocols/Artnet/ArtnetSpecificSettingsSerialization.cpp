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
void JSONObjectReader::read(const Protocols::ArtnetSpecificSettings& n)
{
}

template <>
void JSONObjectWriter::write(Protocols::ArtnetSpecificSettings& n)
{
}
#endif
