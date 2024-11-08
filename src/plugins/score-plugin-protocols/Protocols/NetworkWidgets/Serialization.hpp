#pragma once
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/Metadata.hpp>

#include <ossia/protocols/osc/osc_factory.hpp>

JSON_METADATA(ossia::net::udp_configuration, "UDP")
JSON_METADATA(ossia::net::udp_server_configuration, "UDPServer")
JSON_METADATA(ossia::net::tcp_client_configuration, "TCP")
JSON_METADATA(ossia::net::tcp_server_configuration, "TCPServer")
JSON_METADATA(ossia::net::unix_dgram_configuration, "UnixDatagram")
JSON_METADATA(ossia::net::unix_stream_configuration, "UnixStream")
JSON_METADATA(ossia::net::serial_configuration, "Serial")
JSON_METADATA(ossia::net::ws_client_configuration, "WSClient")
JSON_METADATA(ossia::net::ws_server_configuration, "WSServer")
