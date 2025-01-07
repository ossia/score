// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "BitfocusSpecificSettings.hpp"

#include <State/Value.hpp>
#include <State/ValueSerialization.hpp>

#include <Protocols/NetworkWidgets/Serialization.hpp>

#include <score/serialization/BoostVariant2Serialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const Protocols::BitfocusSpecificSettings& n)
{
  m_stream << n.path << n.entrypoint << n.id << n.name << n.brand << n.product
           << n.nodeVersion << n.apiVersion << n.configuration << n.description;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::BitfocusSpecificSettings& n)
{
  m_stream >> n.path >> n.entrypoint >> n.id >> n.name >> n.brand >> n.product
      >> n.nodeVersion >> n.apiVersion >> n.configuration >> n.description;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::BitfocusSpecificSettings& n)
{
  obj["Path"] = n.path;
  obj["Entrypoint"] = n.entrypoint;
  obj["Identifier"] = n.id;
  obj["Name"] = n.name;
  obj["Brand"] = n.brand;
  obj["Product"] = n.product;
  obj["NodeVersion"] = n.nodeVersion;
  obj["APIVersion"] = n.apiVersion;
  obj["Configuration"] = n.configuration;
  obj["Description"] = n.description;
}

template <>
void JSONWriter::write(Protocols::BitfocusSpecificSettings& n)
{
  n.path <<= obj["Path"];
  n.entrypoint <<= obj["Entrypoint"];
  n.id <<= obj["Identifier"];
  n.name <<= obj["Name"];
  n.brand <<= obj["Brand"];
  n.product <<= obj["Product"];
  n.nodeVersion <<= obj["NodeVersion"];
  n.apiVersion <<= obj["APIVersion"];
  n.configuration <<= obj["Configuration"];
  n.description <<= obj["Description"];
}
