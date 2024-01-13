#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_GPS)
#include "GPSSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/StdVariantSerialization.hpp>

template <>
void DataStreamReader::read(const Protocols::GPSSpecificSettings& n)
{
  m_stream << n.host << n.port;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::GPSSpecificSettings& n)
{
  m_stream >> n.host >> n.port;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::GPSSpecificSettings& n)
{
  obj["Host"] = n.host;
  obj["Port"] = n.port;
}

template <>
void JSONWriter::write(Protocols::GPSSpecificSettings& n)
{
  n.host <<= obj["Host"];
  n.port <<= obj["Port"];
}
#endif
