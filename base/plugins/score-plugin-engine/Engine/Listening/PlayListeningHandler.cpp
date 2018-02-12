// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PlayListeningHandler.hpp"
#include <Explorer/DeviceList.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>

namespace Engine
{
namespace Execution
{
PlayListeningHandler::PlayListeningHandler(
    const Engine::Execution::DocumentPlugin& docpl)
    : m_executor{docpl}
{
}

void PlayListeningHandler::setListening(
    Device::DeviceInterface& dev, const State::Address& addr, bool b)
{
  if (!m_executor.isPlaying())
  {
    dev.setListening(addr, b);
  }
}

void PlayListeningHandler::setListening(
    Device::DeviceInterface& dev, const Device::Node& addr, bool b)
{
  if (!m_executor.isPlaying())
  {
    dev.setListening(Device::address(addr).address, b);
  }
}

void PlayListeningHandler::addToListening(
    Device::DeviceInterface& dev, const std::vector<State::Address>& v)
{
  if (!m_executor.isPlaying())
  {
    dev.addToListening(v);
  }
}
}
}
