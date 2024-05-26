// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CoAPSpecificSettings.hpp"

#include <Protocols/NetworkWidgets/Serialization.hpp>

#include <score/serialization/BoostVariant2Serialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const ossia::net::coap_client_configuration& n)
{
  m_stream << n.transport;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(ossia::net::coap_client_configuration& n)
{
  m_stream >> n.transport;
  checkDelimiter();
}

template <>
void JSONReader::read(const ossia::net::coap_client_configuration& n)
{
  stream.StartObject();
  obj["Transport"] = n.transport;
  stream.EndObject();
}

template <>
void JSONWriter::write(ossia::net::coap_client_configuration& n)
{
  n.transport <<= obj["Transport"];
}

template <>
void DataStreamReader::read(const Protocols::CoAPSpecificSettings& n)
{
  // TODO put it in the right order before 1.0 final.
  // TODO same for minuit, etc..
  m_stream << n.configuration << n.rate;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::CoAPSpecificSettings& n)
{
  m_stream >> n.configuration >> n.rate;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::CoAPSpecificSettings& n)
{
  obj["Config"] = n.configuration;
  if(n.rate)
    obj["Rate"] = *n.rate;
}

template <>
void JSONWriter::write(Protocols::CoAPSpecificSettings& n)
{
  n.configuration <<= obj["Config"];
  if(auto it = obj.tryGet("Rate"))
    n.rate = it->toInt();
}
