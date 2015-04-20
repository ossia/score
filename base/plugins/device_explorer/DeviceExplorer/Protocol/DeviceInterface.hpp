#pragma once
#include <QString>
#include <State/Message.hpp>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>


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

//        virtual void addPath(const AddressSettings& address) = 0;
        virtual void removePath(const QString& path) = 0;

        virtual void sendMessage(Message& mess) = 0;
        virtual bool check(const QString& str) = 0;

    signals:
        void deviceUpdated();

    private:
        DeviceSettings m_settings;


};
