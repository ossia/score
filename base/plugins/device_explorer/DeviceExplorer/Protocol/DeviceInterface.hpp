#pragma once
#include <QString>
#include <State/Message.hpp>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
#include <Plugin/Common/AddressSettings/AddressSettings.hpp>

// Everything here should throw exceptions
// that are to be catched in the document plugin
class DeviceInterface : public QObject
{
        Q_OBJECT

    public:
        DeviceInterface(const DeviceSettings& s):
            m_settings(s)
        {

        }

        const DeviceSettings& settings() const
        { return m_settings; }

        virtual void addPath(const AddressSettings& address) = 0;
        virtual void updatePath(const AddressSettings& address) = 0;
        virtual void removePath(const QString& path) = 0;

        virtual void sendMessage(Message& mess) = 0;
        virtual bool check(const QString& str) = 0;

    signals:
        // These signals are emitted if a device changes from the inside
        void pathAdded(const AddressSettings&);
        void pathUpdated(const AddressSettings&);
        void pathRemoved(const QString&);

    private:
        DeviceSettings m_settings;
};
