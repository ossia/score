#pragma once
#include <OSSIA/Protocols/OSSIADevice.hpp>
#include <Network/Device.h>

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
                const std::shared_ptr<OSSIA::Device>& dev,
                const iscore::DeviceSettings& settings);

        ~LocalDevice();


    private:
        void disconnect() override;
        bool reconnect() override;

        bool canRefresh() const override;
        iscore::Node refresh() override;


        using OSSIADevice::refresh;

        OSSIA::CallbackContainer<OSSIA::Device::AddedNodeCallback>::iterator m_addedNodeCb;
        OSSIA::CallbackContainer<OSSIA::Device::RemovingNodeCallback>::iterator m_removedNodeCb;
        OSSIA::CallbackContainer<OSSIA::Device::NameChangesDeviceCallback>::iterator m_nameChangesCb;
};
