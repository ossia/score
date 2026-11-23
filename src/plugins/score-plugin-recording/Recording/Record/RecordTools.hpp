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
//! those that hold a value
std::vector<State::Address> addressesToRecord(Explorer::DeviceExplorerModel& explorer);
}
