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

        void updateSettings(const iscore::DeviceSettings&) override;
        bool canRefresh() const override;

    private:
        std::shared_ptr<OSSIA::Minuit> m_minuitSettings;
};
