#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>

namespace OSSIA
{
    class Device;
    class Domain;
}

class OSSIADevice : public DeviceInterface
{
    public:
        using DeviceInterface::DeviceInterface;

        void addAddress(const FullAddressSettings& settings) override;
        void updateAddress(const FullAddressSettings& address) override;
        void removeAddress(const QString& path) override;

        void sendMessage(iscore::Message& mess) override;
        bool check(const QString& str) override;

        OSSIA::Device& impl() const;

    protected:
        std::shared_ptr<OSSIA::Device> m_dev;
};
