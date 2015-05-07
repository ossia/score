#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>
#include <API/Headers/Network/Protocol.h>
#include <API/Headers/Network/Device.h>
#include <API/Headers/Network/Address.h>
namespace OSSIA
{
class Device;
}

class OSSIADevice : public DeviceInterface
{
    public:
        using DeviceInterface::DeviceInterface;

        void addAddress(const FullAddressSettings& settings) override;
        void updateAddress(const AddressSettings& address) override;
        void removeAddress(const QString& path) override;

        void sendMessage(Message& mess) override;
        bool check(const QString& str) override;

    protected:
        std::shared_ptr<OSSIA::Device> m_dev;
};
