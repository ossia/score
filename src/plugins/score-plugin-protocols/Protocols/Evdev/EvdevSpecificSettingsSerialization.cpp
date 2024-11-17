#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_EVDEV)
#include "EvdevSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/StdVariantSerialization.hpp>

template <>
void DataStreamReader::read(const Protocols::EvdevSpecificSettings& n)
{
  m_stream << n.name << n.handler << n.bus << n.vendor << n.product << n.version;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::EvdevSpecificSettings& n)
{
  m_stream >> n.name >> n.handler >> n.bus >> n.vendor >> n.product >> n.version;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::EvdevSpecificSettings& n)
{
  obj["Name"] = n.name;
  obj["Handler"] = n.handler;
  obj["Bus"] = n.bus;
  obj["Vendor"] = n.vendor;
  obj["Product"] = n.product;
  obj["Version"] = n.version;
}

template <>
void JSONWriter::write(Protocols::EvdevSpecificSettings& n)
{
  n.name <<= obj["Name"];
  n.handler <<= obj["Handler"];
  n.bus <<= obj["Bus"];
  n.vendor <<= obj["Vendor"];
  n.product <<= obj["Product"];
  n.version <<= obj["Version"];

  // FIXME look for similar device if it ddoes not exist? or at least check the handle
}
#endif
