#pragma once
#include <QString>
#include <State/Message.hpp>
struct AddressSettings
{
    QString path;
    int type;
};

class DeviceInterface : public QObject
{
        Q_OBJECT
    public:
        virtual void addPath(const AddressSettings& address) = 0;
        virtual void removePath(const QString& path) = 0;

        virtual void sendMessage(Message& mess) = 0;
        virtual bool check(const QString& str) = 0;

    signals:
        void deviceUpdated();


};
