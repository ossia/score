#pragma once
#include "Protocols/OSSIADevice.hpp"
#include "MinuitSpecificSettings.hpp"
namespace OSSIA
{
class Device;
}
class MinuitDevice : public OSSIADevice
{
    public:
        MinuitDevice(const DeviceSettings& settings);
        Node refresh() override;
};
