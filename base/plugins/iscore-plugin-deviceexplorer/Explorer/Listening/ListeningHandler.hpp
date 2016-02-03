#pragma once
#include <QObject>
#include <iscore_plugin_deviceexplorer_export.h>
namespace State
{
struct Address;
}
namespace Device
{
class DeviceInterface;
}

namespace Explorer
{
class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT ListeningHandler :
        public QObject
{
        Q_OBJECT
    public:
        virtual ~ListeningHandler();
        virtual void setListening(
                Device::DeviceInterface& dev,
                const State::Address& addr,
                bool b) = 0;

        virtual void replaceListening(
                Device::DeviceInterface& dev,
                const std::vector<State::Address>& v) = 0;

    signals:
        void restoreListening();
};


}
