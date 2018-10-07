#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

class MockDevice : public DeviceInterface
{
    public:
        MockDevice(const score::DeviceSettings& set):
            DeviceInterface(set)
        {

        }

        void updateSettings(const score::DeviceSettings&) override
        {
        }

        bool canRefresh() const override
        {
            return false;
        }

        void setListening(const score::Address&, bool) override
        {
        }

        void replaceListening(const std::vector<score::Address>&) override
        {
        }

        std::vector<score::Address> listening() const override
        {
            return {};
        }

        void addAddress(const score::FullAddressSettings& addr) override
        {
            m_parameters.insert({addr.address, addr});
        }

        void updateAddress(const score::FullAddressSettings& addr) override
        {
            m_parameters.insert({addr.address, addr});
        }

        void removeNode(const score::Address& addr) override
        {
            m_parameters.erase(addr);
        }

        void sendMessage(score::Message mess) override
        {
        }

        bool check(const QString& str) override
        {
            return false;
        }

        std::unordered_map<score::Address, score::FullAddressSettings> m_parameters;
};

class MockDeviceSettings : public ProtocolSettingsWidget
{
    public:
        score::DeviceSettings getSettings() const override
        {
            return {};
        }

        void setSettings(const score::DeviceSettings& settings) override
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

        DeviceInterface* makeDevice(const score::DeviceSettings& settings) override
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
                const score::DeviceSettings& a,
                const score::DeviceSettings& b) const override
        {
            return true;
        }
};

