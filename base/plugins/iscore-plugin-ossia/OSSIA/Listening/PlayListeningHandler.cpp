#include "PlayListeningHandler.hpp"
#include <Device/Protocol/DeviceList.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>

namespace Ossia
{
PlayListeningHandler::PlayListeningHandler(
        const Device::DeviceList &dl,
        const RecreateOnPlay::DocumentPlugin &docpl):
    m_devices{dl},
    m_executor{docpl}
{

}

void PlayListeningHandler::setListening(
        Device::DeviceInterface& dev,
        const State::Address& addr,
        bool b)
{
    if(!m_executor.isPlaying())
    {
        dev.setListening(addr, b);
    }
}

void PlayListeningHandler::addToListening(
        Device::DeviceInterface& dev,
        const std::vector<State::Address>& v)
{
    if(!m_executor.isPlaying())
    {
        dev.addToListening(v);
    }
}
}
