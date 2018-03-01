// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DefaultListeningHandler.hpp"
#include <Explorer/DeviceList.hpp>

namespace Explorer
{
DefaultListeningHandler::DefaultListeningHandler()
{
}

void DefaultListeningHandler::setListening(
    Device::DeviceInterface& dev, const State::Address& addr, bool b)
{
  dev.setListening(addr, b);
}

void DefaultListeningHandler::setListening(Device::DeviceInterface& dev, const Device::Node& addr, bool b)
{
  dev.setListening(Device::address(addr).address, b);
}

void DefaultListeningHandler::addToListening(
    Device::DeviceInterface& dev, const std::vector<State::Address>& v)
{
  dev.addToListening(v);
}
}
