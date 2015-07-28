#pragma once
#include "Protocols/OSSIADevice.hpp"
#include "MinuitSpecificSettings.hpp"
#include <API/Headers/Network/Protocol.h>
#include <QThread>
namespace OSSIA
{
class Device;
}
class MinuitDevice : public OSSIADevice
{
    public:
        MinuitDevice(const iscore::DeviceSettings& settings);
        bool canRefresh() const override;
        iscore::Node refresh() override;
        iscore::Value refresh(const iscore::Address&) override;

    private:
        OSSIA::Minuit m_minuitSettings;
};
