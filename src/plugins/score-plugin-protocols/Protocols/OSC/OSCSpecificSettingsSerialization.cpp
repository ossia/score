// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCSpecificSettings.hpp"

#include <Protocols/NetworkWidgets/Serialization.hpp>

#include <score/serialization/BoostVariant2Serialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(const ossia::net::outbound_socket_configuration& n)
{
  m_stream << n.host << n.port << n.broadcast;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(ossia::net::outbound_socket_configuration& n)
{
  m_stream >> n.host >> n.port >> n.broadcast;
  checkDelimiter();
}

template <>
void JSONReader::read(const ossia::net::outbound_socket_configuration& n)
{
  stream.StartObject();
  obj["Host"] = n.host;
  obj["Port"] = (int)n.port;
  obj["Broadcast"] = n.broadcast;
  stream.EndObject();
}

template <>
void JSONWriter::write(ossia::net::outbound_socket_configuration& n)
{
  n.host = obj["Host"].toStdString();
  n.port = obj["Port"].toInt();
  if(auto bc = obj.tryGet("Broadcast"))
    n.broadcast = bc->toBool();
}

template <>
void DataStreamReader::read(const ossia::net::inbound_socket_configuration& n)
{
  m_stream << n.bind << n.port;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(ossia::net::inbound_socket_configuration& n)
{
  m_stream >> n.bind >> n.port;
  checkDelimiter();
}

template <>
void JSONReader::read(const ossia::net::inbound_socket_configuration& n)
{
  stream.StartObject();
  obj["Bind"] = n.bind;
  obj["Port"] = (int)n.port;
  stream.EndObject();
}

template <>
void JSONWriter::write(ossia::net::inbound_socket_configuration& n)
{
  if(auto b = obj.tryGet("Bind"))
    n.bind = b->toStdString();
  else if(auto h = obj.tryGet("Host"))
    n.bind = h->toStdString();
  n.port = obj["Port"].toInt();
}

template <>
void DataStreamReader::read(const ossia::net::fd_configuration& n)
{
  m_stream << n.fd;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(ossia::net::fd_configuration& n)
{
  m_stream >> n.fd;
  checkDelimiter();
}

template <>
void JSONReader::read(const ossia::net::fd_configuration& n)
{
  stream.StartObject();
  obj["Path"] = n.fd;
  stream.EndObject();
}

template <>
void JSONWriter::write(ossia::net::fd_configuration& n)
{
  n.fd = obj["Path"].toStdString();
}

template <>
void DataStreamReader::read(const ossia::net::ws_client_configuration& n)
{
  m_stream << n.url;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(ossia::net::ws_client_configuration& n)
{
  m_stream >> n.url;
  checkDelimiter();
}

template <>
void JSONReader::read(const ossia::net::ws_client_configuration& n)
{
  stream.StartObject();
  obj["Url"] = n.url;
  stream.EndObject();
}

template <>
void JSONWriter::write(ossia::net::ws_client_configuration& n)
{
  n.url = obj["Url"].toStdString();
}

template <>
void DataStreamReader::read(const ossia::net::ws_server_configuration& n)
{
  m_stream << n.port;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(ossia::net::ws_server_configuration& n)
{
  m_stream >> n.port;
  checkDelimiter();
}

template <>
void JSONReader::read(const ossia::net::ws_server_configuration& n)
{
  stream.StartObject();
  obj["Port"] = n.port;
  stream.EndObject();
}

template <>
void JSONWriter::write(ossia::net::ws_server_configuration& n)
{
  n.port = obj["Port"].toInt();
}

template <>
void DataStreamReader::read(const ossia::net::serial_configuration& n)
{
  m_stream << n.port << n.baud_rate << n.character_size << n.flow_control << n.parity
           << n.stop_bits;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(ossia::net::serial_configuration& n)
{
  m_stream >> n.port >> n.baud_rate >> n.character_size >> n.flow_control >> n.parity
      >> n.stop_bits;
  checkDelimiter();
}

template <>
void JSONReader::read(const ossia::net::serial_configuration& n)
{
  stream.StartObject();
  obj["Port"] = n.port;
  obj["BaudRate"] = n.baud_rate;
  obj["CharSize"] = n.character_size;
  obj["FlowControl"] = n.flow_control;
  obj["Parity"] = n.parity;
  obj["StopBits"] = n.stop_bits;
  stream.EndObject();
}

template <>
void JSONWriter::write(ossia::net::serial_configuration& n)
{
  n.port = obj["Port"].toStdString();
  n.baud_rate = obj["BaudRate"].toInt();
  n.character_size = obj["CharSize"].toInt();
  n.flow_control <<= obj["FlowControl"];
  n.parity <<= obj["Parity"];
  n.stop_bits <<= obj["StopBits"];
}

template <>
void DataStreamReader::read(const ossia::net::receive_fd_configuration& n)
{
  read((ossia::net::fd_configuration&)n);
}

template <>
void DataStreamWriter::write(ossia::net::receive_fd_configuration& n)
{
  write((ossia::net::fd_configuration&)n);
}

template <>
void JSONReader::read(const ossia::net::receive_fd_configuration& n)
{
  read((ossia::net::fd_configuration&)n);
}

template <>
void JSONWriter::write(ossia::net::receive_fd_configuration& n)
{
  write((ossia::net::fd_configuration&)n);
}

template <>
void DataStreamReader::read(const ossia::net::send_fd_configuration& n)
{
  read((ossia::net::fd_configuration&)n);
}

template <>
void DataStreamWriter::write(ossia::net::send_fd_configuration& n)
{
  write((ossia::net::fd_configuration&)n);
}

template <>
void JSONReader::read(const ossia::net::send_fd_configuration& n)
{
  read((ossia::net::fd_configuration&)n);
}

template <>
void JSONWriter::write(ossia::net::send_fd_configuration& n)
{
  write((ossia::net::fd_configuration&)n);
}

template <>
void DataStreamReader::read(const ossia::net::unix_stream_configuration& n)
{
  read((ossia::net::fd_configuration&)n);
}

template <>
void DataStreamWriter::write(ossia::net::unix_stream_configuration& n)
{
  write((ossia::net::fd_configuration&)n);
}

template <>
void JSONReader::read(const ossia::net::unix_stream_configuration& n)
{
  read((ossia::net::fd_configuration&)n);
}

template <>
void JSONWriter::write(ossia::net::unix_stream_configuration& n)
{
  write((ossia::net::fd_configuration&)n);
}

template <>
void DataStreamReader::read(const ossia::net::tcp_client_configuration& n)
{
  read((ossia::net::outbound_socket_configuration&)n);
}

template <>
void DataStreamWriter::write(ossia::net::tcp_client_configuration& n)
{
  write((ossia::net::outbound_socket_configuration&)n);
}

template <>
void JSONReader::read(const ossia::net::tcp_client_configuration& n)
{
  read((ossia::net::outbound_socket_configuration&)n);
}

template <>
void JSONWriter::write(ossia::net::tcp_client_configuration& n)
{
  write((ossia::net::outbound_socket_configuration&)n);
}

template <>
void DataStreamReader::read(const ossia::net::tcp_server_configuration& n)
{
  read((ossia::net::inbound_socket_configuration&)n);
}

template <>
void DataStreamWriter::write(ossia::net::tcp_server_configuration& n)
{
  write((ossia::net::inbound_socket_configuration&)n);
}

template <>
void JSONReader::read(const ossia::net::tcp_server_configuration& n)
{
  read((ossia::net::inbound_socket_configuration&)n);
}

template <>
void JSONWriter::write(ossia::net::tcp_server_configuration& n)
{
  write((ossia::net::inbound_socket_configuration&)n);
}

template <>
void DataStreamReader::read(const ossia::net::udp_configuration& n)
{
  m_stream << n.local << n.remote;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(ossia::net::udp_configuration& n)
{
  m_stream >> n.local >> n.remote;
  checkDelimiter();
}

template <>
void JSONReader::read(const ossia::net::udp_configuration& n)
{
  stream.StartObject();
  obj["Local"] = n.local;
  obj["Remote"] = n.remote;
  stream.EndObject();
}

template <>
void JSONWriter::write(ossia::net::udp_configuration& n)
{
  n.local <<= obj["Local"];
  n.remote <<= obj["Remote"];
}

template <>
void DataStreamReader::read(const ossia::net::unix_dgram_configuration& n)
{
  m_stream << n.local << n.remote;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(ossia::net::unix_dgram_configuration& n)
{
  m_stream >> n.local >> n.remote;
  checkDelimiter();
}

template <>
void JSONReader::read(const ossia::net::unix_dgram_configuration& n)
{
  stream.StartObject();
  obj["Local"] = n.local;
  obj["Remote"] = n.remote;
  stream.EndObject();
}

template <>
void JSONWriter::write(ossia::net::unix_dgram_configuration& n)
{
  n.local <<= obj["Local"];
  n.remote <<= obj["Remote"];
}

template <>
void DataStreamReader::read(const ossia::net::osc_protocol_configuration& n)
{
  m_stream << n.mode << n.version << n.framing << n.transport;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(ossia::net::osc_protocol_configuration& n)
{
  m_stream >> n.mode >> n.version >> n.framing >> n.transport;
  checkDelimiter();
}

template <>
void JSONReader::read(const ossia::net::osc_protocol_configuration& n)
{
  stream.StartObject();
  obj["Mode"] = n.mode;
  obj["Version"] = n.version;
  obj["Framing"] = n.framing;
  obj["Transport"] = n.transport;
  stream.EndObject();
}

template <>
void JSONWriter::write(ossia::net::osc_protocol_configuration& n)
{
  n.mode <<= obj["Mode"];
  n.version <<= obj["Version"];
  n.framing <<= obj["Framing"];
  n.transport <<= obj["Transport"];
}

template <>
void DataStreamReader::read(const Protocols::OSCSpecificSettings& n)
{
  // TODO put it in the right order before 1.0 final.
  // TODO same for minuit, etc..
  m_stream << n.configuration << n.rate << n.bonjour << n.jsonToLoad << n.oscquery;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Protocols::OSCSpecificSettings& n)
{
  m_stream >> n.configuration >> n.rate >> n.bonjour >> n.jsonToLoad >> n.oscquery;
  checkDelimiter();
}

template <>
void JSONReader::read(const Protocols::OSCSpecificSettings& n)
{
  obj["Config"] = n.configuration;
  if(n.rate)
    obj["Rate"] = *n.rate;
  if(n.bonjour)
    obj["Bonjour"] = true;
  if(n.oscquery)
    obj["OSCQuery"] = *n.oscquery;
}

template <>
void JSONWriter::write(Protocols::OSCSpecificSettings& n)
{
  // Old save format
  if(auto outputPort = obj.tryGet("OutputPort"))
  {
    ossia::net::udp_configuration conf;
    conf.local = ossia::net::inbound_socket_configuration{
        "0.0.0.0", (uint16_t)obj["OutputPort"].toInt()};
    conf.remote = ossia::net::outbound_socket_configuration{
        obj["Host"].toStdString(), (uint16_t)obj["InputPort"].toInt()};
    n.configuration.mode = ossia::net::osc_protocol_configuration::MIRROR;
    n.configuration.version = ossia::net::osc_protocol_configuration::OSC1_0;
    n.configuration.transport = std::move(conf);
  }
  else
  {
    n.configuration <<= obj["Config"];
  }

  if(auto it = obj.tryGet("Rate"))
    n.rate = it->toInt();

  assign_with_default(n.bonjour, obj.tryGet("Bonjour"), false);

  if(auto it = obj.tryGet("OSCQuery"))
    n.oscquery = it->toInt();
}
