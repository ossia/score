#pragma once
#include "Protocols/OSSIADevice.hpp"
#include "OSCSpecificSettings.hpp"

class OSCDevice final : public OSSIADevice
{
    public:
        OSCDevice(const iscore::DeviceSettings& stngs);
        void updateSettings(const iscore::DeviceSettings& settings) override;
};
