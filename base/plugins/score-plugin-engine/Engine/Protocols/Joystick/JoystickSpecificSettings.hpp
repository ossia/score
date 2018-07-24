#pragma once

#include <QMetaType>
#include <wobjectdefs.h>

namespace Engine {

    namespace Network {

        struct JoystickSpecificSettings {
            int32_t joystick_id;
            int joystick_index;
        };
    }
}

Q_DECLARE_METATYPE(Engine::Network::JoystickSpecificSettings)
W_REGISTER_ARGTYPE(Engine::Network::JoystickSpecificSettings)
