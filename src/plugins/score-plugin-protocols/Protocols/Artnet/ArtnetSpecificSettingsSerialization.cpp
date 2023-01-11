#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include "ArtnetSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/StdVariantSerialization.hpp>

JSON_METADATA(Protocols::Artnet::SingleCapability, "SingleCapability")
JSON_METADATA(std::vector<Protocols::Artnet::RangeCapability>, "RangeCapabilities")

template <>
void DataStreamReader::read(const Protocols::Artnet::SingleCapability& n)
{
  m_stream << n.comment << n.type << n.effectName;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::Artnet::SingleCapability& n)
{
  m_stream >> n.comment >> n.type >> n.effectName;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::Artnet::SingleCapability& n)
{
  stream.StartObject();
  obj["Type"] = n.type;
  if(!n.comment.isEmpty())
    obj["Comment"] = n.comment;
  if(!n.effectName.isEmpty())
    obj["EffectName"] = n.comment;
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::Artnet::SingleCapability& n)
{
  n.type <<= obj["Type"];
  if(auto val = obj.tryGet("Comment"))
    n.comment <<= *val;
  if(auto val = obj.tryGet("EffectName"))
    n.effectName <<= *val;
}

template <>
void DataStreamReader::read(const Protocols::Artnet::RangeCapability& n)
{
  m_stream << n.comment << n.type << n.effectName;
  m_stream << n.range;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::Artnet::RangeCapability& n)
{
  m_stream >> n.comment >> n.type >> n.effectName;
  m_stream >> n.range;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::Artnet::RangeCapability& n)
{
  stream.StartObject();
  obj["Type"] = n.type;
  if(!n.comment.isEmpty())
    obj["Comment"] = n.comment;
  if(!n.effectName.isEmpty())
    obj["EffectName"] = n.comment;

  obj["Range"] = n.range;
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::Artnet::RangeCapability& n)
{
  n.type <<= obj["Type"];
  if(auto val = obj.tryGet("Comment"))
    n.comment <<= *val;
  if(auto val = obj.tryGet("EffectName"))
    n.effectName <<= *val;

  n.range <<= obj["Range"];
}

template <>
void DataStreamReader::read(const Protocols::Artnet::Channel& n)
{
  m_stream << n.name << n.defaultValue << n.capabilities << n.fineChannels;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::Artnet::Channel& n)
{
  m_stream >> n.name >> n.defaultValue >> n.capabilities >> n.fineChannels;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::Artnet::Channel& n)
{
  stream.StartObject();
  obj["Name"] = n.name;
  obj["DefaultValue"] = n.defaultValue;
  obj["Capabilities"] = n.capabilities;
  obj["FineChannels"] = n.fineChannels;
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::Artnet::Channel& n)
{
  n.name <<= obj["Name"];
  n.defaultValue <<= obj["DefaultValue"];
  n.capabilities <<= obj["Capabilities"];
  if(auto fc = obj.tryGet("FineChannels"))
    n.fineChannels <<= *fc;
}

template <>
void DataStreamReader::read(const Protocols::Artnet::ModeInfo& n)
{
  m_stream << n.channelNames;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::Artnet::ModeInfo& n)
{
  m_stream >> n.channelNames;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::Artnet::ModeInfo& n)
{
  stream.StartObject();
  obj["ChannelNames"] = n.channelNames;
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::Artnet::ModeInfo& n)
{
  n.channelNames <<= obj["ChannelNames"];
}

template <>
void DataStreamReader::read(const Protocols::Artnet::Fixture& n)
{
  m_stream << n.fixtureName << n.modeName << n.mode << n.controls << n.address;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::Artnet::Fixture& n)
{
  m_stream >> n.fixtureName >> n.modeName >> n.mode >> n.controls >> n.address;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::Artnet::Fixture& n)
{
  stream.StartObject();
  obj["Name"] = n.fixtureName;
  obj["Mode"] = n.modeName;
  obj["ModeInfo"] = n.mode;
  obj["Address"] = n.address;
  obj["Channels"] = n.controls;
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::Artnet::Fixture& n)
{
  n.fixtureName <<= obj["Name"];
  n.modeName <<= obj["Mode"];
  if(auto mi = obj.tryGet("ModeInfo"))
    n.mode <<= *mi;
  n.address <<= obj["Address"];
  n.controls <<= obj["Channels"];
}

template <>
void DataStreamReader::read(const Protocols::ArtnetSpecificSettings& n)
{
  m_stream << n.fixtures << n.host << n.rate << n.universe << n.transport;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::ArtnetSpecificSettings& n)
{
  m_stream >> n.fixtures >> n.host >> n.rate >> n.universe >> n.transport;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::ArtnetSpecificSettings& n)
{
  obj["Fixtures"] = n.fixtures;
  obj["Host"] = n.host;
  obj["Rate"] = n.rate;
  obj["Universe"] = n.universe;
  obj["Transport"] = n.transport;
}

template <>
void JSONWriter::write(Protocols::ArtnetSpecificSettings& n)
{
  n.fixtures <<= obj["Fixtures"];
  if(auto u = obj.tryGet("Host"))
    n.host = QString::fromStdString(u->toStdString());
  n.rate <<= obj["Rate"];
  if(auto u = obj.tryGet("Universe"))
    n.universe = u->toInt();
  if(auto u = obj.tryGet("Transport"))
    n.transport = (decltype(n.transport))u->toInt();
}
#endif
