#include "DefaultListeningHandler.hpp"
#include <Device/Protocol/DeviceList.hpp>

namespace Explorer
{
DefaultListeningHandler::DefaultListeningHandler(
        const Device::DeviceList &dl):
    m_devices{dl}
{

}

void DefaultListeningHandler::setListening(
        Device::DeviceInterface& dev,
        const State::Address &addr,
        bool b)
{
    dev.setListening(addr, b);
}

void DefaultListeningHandler::addToListening(
        Device::DeviceInterface& dev,
        const std::vector<State::Address> &v)
{
    dev.addToListening(v);
}
}
