#pragma once
#include "Protocols/OSSIADevice.hpp"

namespace iscore {
struct DeviceSettings;
}  // namespace iscore

class OSCDevice final : public OSSIADevice
{
    public:
        OSCDevice(const iscore::DeviceSettings& stngs);

        bool reconnect() override;
};
