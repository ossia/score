#pragma once
#include <Explorer/Listening/ListeningHandler.hpp>
namespace Device
{
class DeviceList;
}
namespace Explorer
{
class DefaultListeningHandler final :
        public ListeningHandler
{
        const Device::DeviceList& m_devices;

    public:
        DefaultListeningHandler(
                const Device::DeviceList& dl);

    private:
        void setListening(
                Device::DeviceInterface& dev,
                const State::Address& addr,
                bool b) override;

        void addToListening(
                Device::DeviceInterface& dev,
                const std::vector<State::Address>& v) override;
};
}
