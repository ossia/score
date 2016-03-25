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
    public:
        DefaultListeningHandler();

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
