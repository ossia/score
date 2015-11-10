#pragma once
#include "Protocols/OSSIADevice.hpp"
#include "MinuitSpecificSettings.hpp"
#include <API/Headers/Network/Protocol/Minuit.h>
#include <QThread>
namespace OSSIA
{
class Device;
}
class MinuitDevice final : public OSSIADevice
{
    public:
        MinuitDevice(const iscore::DeviceSettings& settings);

        bool reconnect() override;

        bool canRefresh() const override;
};
