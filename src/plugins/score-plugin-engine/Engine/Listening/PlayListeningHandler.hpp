#pragma once
#include <Explorer/Listening/ListeningHandler.hpp>

namespace Device
{
class DeviceList;
}
namespace Execution
{
class DocumentPlugin;

class PlayListeningHandler final : public Explorer::ListeningHandler
{
  const Execution::DocumentPlugin& m_executor;

public:
  PlayListeningHandler(const Execution::DocumentPlugin& docpl);

private:
  void setListening(Device::DeviceInterface& dev, const State::Address& addr, bool b) override;

  void setListening(Device::DeviceInterface& dev, const Device::Node& addr, bool b) override;

  void addToListening(Device::DeviceInterface& dev, const std::vector<State::Address>& v) override;
};
}
