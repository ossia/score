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
                const iscore::DocumentContext& ctx,
                std::shared_ptr<OSSIA::Device> dev,
                const iscore::DeviceSettings& settings);


    private:
        bool reconnect() override;

        bool canRefresh() const override;
        iscore::Node refresh() override;
        using OSSIADevice::refresh;

        //const iscore::DocumentContext& m_context;
};
