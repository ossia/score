#pragma once

#include <QMetaType>

#include <wobjectdefs.h>

namespace Engine::Network
{

struct JoystickSpecificSettings
{
  int32_t joystick_id;
  int32_t joystick_index;
};
}

Q_DECLARE_METATYPE(Engine::Network::JoystickSpecificSettings)
W_REGISTER_ARGTYPE(Engine::Network::JoystickSpecificSettings)
