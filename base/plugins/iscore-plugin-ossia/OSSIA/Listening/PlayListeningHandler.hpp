#pragma once
#include <Explorer/Listening/ListeningHandler.hpp>

namespace Device
{
class DeviceList;
}
namespace RecreateOnPlay
{
class DocumentPlugin;
}
namespace Ossia
{
class PlayListeningHandler final : public Explorer::ListeningHandler
{
        const Device::DeviceList& m_devices;
        const RecreateOnPlay::DocumentPlugin& m_executor;

    public:
        PlayListeningHandler(
                const Device::DeviceList& dl,
                const RecreateOnPlay::DocumentPlugin& docpl);

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
