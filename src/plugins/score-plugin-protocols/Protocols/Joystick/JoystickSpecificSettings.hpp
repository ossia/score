#pragma once

#include <verdigris>

namespace Protocols
{

struct JoystickSpecificSettings
{
  score::uuid_t id{};

  // device id, device index
  static const constexpr std::pair<int32_t, int32_t> unassigned{-1, -1};
  std::pair<int32_t, int32_t> spec{unassigned};
};
}

Q_DECLARE_METATYPE(Protocols::JoystickSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::JoystickSpecificSettings)
