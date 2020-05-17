#pragma once

#include <verdigris>

namespace Protocols
{

struct JoystickSpecificSettings
{
  int32_t joystick_id;
  int32_t joystick_index;
};
}

Q_DECLARE_METATYPE(Protocols::JoystickSpecificSettings)
W_REGISTER_ARGTYPE(Protocols::JoystickSpecificSettings)
