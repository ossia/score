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

        void addAddress(const iscore::FullAddressSettings& settings) override;
        void updateAddress(const iscore::FullAddressSettings& address) override;
        void removeAddress(const iscore::Address& path) override;

        void sendMessage(iscore::Message mess) override;
        bool check(const QString& str) override;

        OSSIA::Device& impl() const;
        std::shared_ptr<OSSIA::Device> impl_ptr() const
        { return m_dev; }

    protected:
        std::shared_ptr<OSSIA::Device> m_dev;
};
