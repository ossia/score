#pragma once
#include <DeviceExplorer/Protocol/DeviceInterface.hpp>
#include "MinuitSpecificSettings.hpp"
class MinuitDevice : public DeviceInterface
{
    public:
        MinuitDevice(const DeviceSettings& settings);

        void addAddress(const AddressSettings& address) override;
        void updateAddress(const AddressSettings& address) override;
        void removeAddress(const QString& path) override;

        void sendMessage(Message& mess) override;
        bool check(const QString& str) override;
};
