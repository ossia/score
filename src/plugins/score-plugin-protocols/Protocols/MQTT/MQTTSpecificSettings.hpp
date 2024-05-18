#pragma once
#include <ossia/detail/optional.hpp>
#include <ossia/protocols/mqtt/mqtt_protocol.hpp>

#include <QString>

#include <verdigris>

namespace Protocols
{
struct MQTTSpecificSettings
{
  ossia::net::mqtt5_configuration configuration;
  std::optional<int> rate{};
};
}
Q_DECLARE_METATYPE(Protocols::MQTTSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::MQTTSpecificSettings)
