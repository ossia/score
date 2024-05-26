#pragma once
#include <ossia/detail/optional.hpp>
#include <ossia/protocols/coap/coap_client_protocol.hpp>

#include <QString>

#include <verdigris>

namespace Protocols
{
struct CoAPSpecificSettings
{
  ossia::net::coap_client_configuration configuration;
  std::optional<int> rate{};
};
}
Q_DECLARE_METATYPE(Protocols::CoAPSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::CoAPSpecificSettings)
