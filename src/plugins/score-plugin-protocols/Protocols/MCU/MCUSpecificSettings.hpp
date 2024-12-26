#pragma once
#include <QString>

#include <libremidi/api.hpp>
#include <libremidi/observer_configuration.hpp>

#include <verdigris>

namespace Protocols
{
struct MCUSpecificSettings
{
  std::vector<libremidi::input_port> input_handle;
  std::vector<libremidi::output_port> output_handle;
  libremidi::API api{};

  enum
  {
    MCU,
    Generic
  } mode{};
};
}
Q_DECLARE_METATYPE(Protocols::MCUSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::MCUSpecificSettings)
