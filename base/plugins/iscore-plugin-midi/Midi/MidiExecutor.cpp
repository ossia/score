#include "MidiExecutor.hpp"
#include <Midi/MidiProcess.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/iscore2OSSIA.hpp>
namespace Midi
{
namespace Executor
{
ProcessExecutor::ProcessExecutor(
        const Midi::ProcessModel& proc,
        const Device::DeviceList& devices):
    m_process{proc},
    m_devices{devices}
{
    /*
    // Load the address
    // Look for the real node in the device
    auto dev_it = devices.find(addr.device);
    if(dev_it == devices.devices().end())
        return;

    auto dev = dynamic_cast<Engine::Network::MIDIDevice*>(*dev_it);
    if(!dev)
        return;

    auto node = Engine::iscore_to_ossia::findNodeFromPath(addr.path, *dev->getDevice());
    if(!node)
        return;

    // Add the real address
    m_addr = node->getAddress();
    if(!m_addr)
        return;

    */
}

ProcessExecutor::~ProcessExecutor()
{
}

ossia::state_element ProcessExecutor::state()
{
    return state(parent->getPosition());
}

ossia::state_element ProcessExecutor::state(double t)
{
    return {};
}

ossia::state_element ProcessExecutor::offset(ossia::time_value off)
{
    return state(off / parent->getDurationNominal());
}

}
}
