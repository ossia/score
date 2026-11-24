#pragma once
#include <State/Address.hpp>

#include <vector>

namespace Explorer
{
class DeviceExplorerModel;
}

namespace Recording
{
//! The addresses selected in the explorer and every address under them,
//! audio and texture inputs included
std::vector<State::Address> addressesToRecord(Explorer::DeviceExplorerModel& explorer);
}
