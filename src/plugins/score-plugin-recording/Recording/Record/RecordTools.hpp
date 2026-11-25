#pragma once
#include <State/Address.hpp>

#include <vector>

namespace Explorer
{
class DeviceExplorerModel;
class DeviceDocumentPlugin;
}

namespace Recording
{
//! The addresses selected in the explorer and every address under them,
//! audio and texture inputs included
std::vector<State::Address> addressesToRecord(Explorer::DeviceExplorerModel& explorer);

//! What addresses record as
struct AddressKinds
{
  //! Numbers, strings...: automations or messages
  bool values{};
  //! MIDI devices: notes
  bool midi{};
  //! Audio and video inputs: files
  bool streams{};
};
AddressKinds addressKinds(
    Explorer::DeviceDocumentPlugin& devices, const std::vector<State::Address>& addresses);
}
