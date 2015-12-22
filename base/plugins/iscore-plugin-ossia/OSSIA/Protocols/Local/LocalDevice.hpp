#pragma once
#include <OSSIA/Protocols/OSSIADevice.hpp>

namespace iscore {
struct DeviceSettings;
}  // namespace iscore

namespace OSSIA
{
}
class LocalDevice final : public OSSIADevice
{
    public:
        LocalDevice(
                std::shared_ptr<OSSIA::Device> dev,
                const iscore::DeviceSettings& settings);

        bool reconnect() override;

        bool canRefresh() const override;
};
