#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

class MockDevice : public DeviceInterface
{
    public:
        MockDevice(const iscore::DeviceSettings& set):
            DeviceInterface(set)
        {

        }

        void updateSettings(const iscore::DeviceSettings&) override
        {
        }

        bool canRefresh() const override
        {
            return false;
        }

        void setListening(const iscore::Address&, bool) override
        {
        }

        void replaceListening(const std::vector<iscore::Address>&) override
        {
        }

        std::vector<iscore::Address> listening() const override
        {
            return {};
        }

        void addAddress(const iscore::FullAddressSettings& addr) override
        {
            m_addresses.insert({addr.address, addr});
        }

        void updateAddress(const iscore::FullAddressSettings& addr) override
        {
            m_addresses.insert({addr.address, addr});
        }

        void removeNode(const iscore::Address& addr) override
        {
            m_addresses.erase(addr);
        }

        void sendMessage(iscore::Message mess) override
        {
        }

        bool check(const QString& str) override
        {
            return false;
        }

        std::unordered_map<iscore::Address, iscore::FullAddressSettings> m_addresses;
};

class MockDeviceSettings : public ProtocolSettingsWidget
{
    public:
        iscore::DeviceSettings getSettings() const override
        {
            return {};
        }

        QString getPath() const override
        {
            return {};
        }

        void setSettings(const iscore::DeviceSettings& settings) override
        {
        }
};

class MockDeviceFactory : public ProtocolFactory
{
    public:
        QString name() const override
        {
            return "Mock";
        }

        DeviceInterface* makeDevice(const iscore::DeviceSettings& settings) override
        {
            return new MockDevice(settings);
        }

        ProtocolSettingsWidget* makeSettingsWidget() override
        {
            return new MockDeviceSettings;
        }

        void serializeProtocolSpecificSettings(
                const QVariant& data,
                const VisitorVariant& visitor) const override
        {
        }

        QVariant makeProtocolSpecificSettings(
                const VisitorVariant& visitor) const override
        {
            return {};
        }

        bool checkCompatibility(
                const iscore::DeviceSettings& a,
                const iscore::DeviceSettings& b) const override
        {
            return true;
        }
};

