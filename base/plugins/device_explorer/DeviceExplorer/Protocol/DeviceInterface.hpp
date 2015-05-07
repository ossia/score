#pragma once
#include <QString>
#include <State/Message.hpp>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
#include <Plugin/Common/AddressSettings/AddressSettings.hpp>
#include <memory>

class DeviceInterface : public QObject
{
        Q_OBJECT

    public:
        DeviceInterface(const DeviceSettings& s);
        const DeviceSettings& settings() const;

        virtual void addAddress(const FullAddressSettings& address) = 0;
        virtual void updateAddress(const FullAddressSettings& address) = 0;
        virtual void removeAddress(const QString& path) = 0;

        // Execution API... Maybe we don't need it here.
        virtual void sendMessage(Message& mess) = 0;
        virtual bool check(const QString& str) = 0;

    signals:
        // These signals are emitted if a device changes from the inside
        void pathAdded(const FullAddressSettings&);
        void pathUpdated(const FullAddressSettings&);
        void pathRemoved(const QString&);

        // In case the whole namespace changed?
        void namespaceUpdated();

    private:
        DeviceSettings m_settings;
};
