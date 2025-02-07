#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include "ArtnetSpecificSettings.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/StdVariantSerialization.hpp>
#include <score/serialization/VariantSerialization.hpp>

JSON_METADATA(Protocols::Artnet::SingleCapability, "SingleCapability")
JSON_METADATA(std::vector<Protocols::Artnet::RangeCapability>, "RangeCapabilities")
JSON_METADATA(Protocols::Artnet::LEDStripLayout, "LEDStripLayout")
JSON_METADATA(Protocols::Artnet::LEDPaneLayout, "LEDPaneLayout")
JSON_METADATA(Protocols::Artnet::LEDVolumeLayout, "LEDVolumeLayout")

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
void DataStreamReader::read(const Protocols::Artnet::LEDStripLayout& n)
{
  m_stream << n.diodes << n.length << n.reverse;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::Artnet::LEDStripLayout& n)
{
  m_stream >> n.diodes >> n.length >> n.reverse;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::Artnet::LEDStripLayout& n)
{
  stream.StartObject();
  obj["Diodes"] = n.diodes;
  obj["Length"] = n.length;
  obj["Reverse"] = n.reverse;
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::Artnet::LEDStripLayout& n)
{
  n.diodes <<= obj["Diodes"];
  n.length <<= obj["Length"];
  n.reverse <<= obj["Reverse"];
}

template <>
void DataStreamReader::read(const Protocols::Artnet::LEDPaneLayout& n)
{
  m_stream << n.diodes << n.width << n.height;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::Artnet::LEDPaneLayout& n)
{
  m_stream >> n.diodes >> n.width >> n.height;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::Artnet::LEDPaneLayout& n)
{
  stream.StartObject();
  obj["Diodes"] = n.diodes;
  obj["Width"] = n.width;
  obj["Height"] = n.height;
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::Artnet::LEDPaneLayout& n)
{
  n.diodes <<= obj["Diodes"];
  n.width <<= obj["Width"];
  n.height <<= obj["Height"];
}

template <>
void DataStreamReader::read(const Protocols::Artnet::LEDVolumeLayout& n)
{
  m_stream << n.diodes << n.width << n.height << n.depth;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::Artnet::LEDVolumeLayout& n)
{
  m_stream >> n.diodes >> n.width >> n.height >> n.depth;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::Artnet::LEDVolumeLayout& n)
{
  stream.StartObject();
  obj["Diodes"] = n.diodes;
  obj["Width"] = n.width;
  obj["Height"] = n.height;
  obj["Depth"] = n.depth;
  stream.EndObject();
}

template <>
void JSONWriter::write(Protocols::Artnet::LEDVolumeLayout& n)
{
  n.diodes <<= obj["Diodes"];
  n.width <<= obj["Width"];
  n.height <<= obj["Height"];
  n.depth <<= obj["Depth"];
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
  m_stream << n.fixtureName << n.modeName << n.mode << n.controls << n.led << n.address;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::Artnet::Fixture& n)
{
  m_stream >> n.fixtureName >> n.modeName >> n.mode >> n.controls >> n.led >> n.address;
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
  if(!n.controls.empty())
    obj["Channels"] = n.controls;
  else if(n.led)
    obj["LED"] = n.led;
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
  if(auto ctls = obj.tryGet("Channels"))
    n.controls <<= *ctls;
  else if(auto led = obj.tryGet("LED"))
    n.led <<= *led;
}

template <>
void DataStreamReader::read(const Protocols::ArtnetSpecificSettings& n)
{
  m_stream << n.fixtures << n.host << n.rate << n.universe << n.channels_per_universe
           << n.multicast << n.transport << n.mode;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::ArtnetSpecificSettings& n)
{
  m_stream >> n.fixtures >> n.host >> n.rate >> n.universe >> n.channels_per_universe
      >> n.multicast >> n.transport >> n.mode;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::ArtnetSpecificSettings& n)
{
  obj["Fixtures"] = n.fixtures;
  obj["Host"] = n.host;
  obj["Rate"] = n.rate;
  obj["Universe"] = n.universe;
  obj["ChannelsPerUniverse"] = n.channels_per_universe;
  if(n.multicast)
    obj["Multicast"] = n.multicast;
  obj["Transport"] = n.transport;
  obj["Mode"] = n.mode;
}

template <>
void JSONWriter::write(Protocols::ArtnetSpecificSettings& n)
{
  if(auto fixt = obj.tryGet("Fixtures"))
    n.fixtures <<= *fixt;
  if(auto u = obj.tryGet("Host"))
    n.host = QString::fromStdString(u->toStdString());
  n.rate <<= obj["Rate"];
  if(auto u = obj.tryGet("Universe"))
    n.universe = u->toInt();
  if(auto u = obj.tryGet("ChannelsPerUniverse"))
    n.channels_per_universe = u->toInt();
  if(auto u = obj.tryGet("Multicast"))
    n.multicast = u->toBool();
  if(auto u = obj.tryGet("Transport"))
    n.transport = (decltype(n.transport))u->toInt();
  if(auto u = obj.tryGet("Mode"))
    n.mode = (decltype(n.mode))u->toInt();
}
#endif
