#pragma once
#include <QString>
#include <State/Message.hpp>
class AddressSettings
{

};

class DeviceInterface
{
    public:
        virtual void addPath(const AddressSettings& address) = 0;
        virtual void removePath(const QString& path) = 0;

        virtual void sendMessage(Message& mess) = 0;
        virtual bool check(const QString& str) = 0;
};
