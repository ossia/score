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
        bool canRefresh() const override;
        Node refresh() override;
};
