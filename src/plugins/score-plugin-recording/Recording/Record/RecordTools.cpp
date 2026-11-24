#include "RecordTools.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

namespace Recording
{
namespace
{
void parametersUnder(Device::Node& parent, std::vector<Device::Node*>& vec)
{
  for(auto& node : parent)
  {
    vec.push_back(&node);
    parametersUnder(node, vec);
  }
}
}

std::vector<State::Address> addressesToRecord(Explorer::DeviceExplorerModel& explorer)
{
  auto nodes = explorer.uniqueSelectedNodes(explorer.selectedIndexes());
  std::vector<Device::Node*> all;
  for(auto* node : nodes.parents.empty() ? nodes.messages : nodes.parents)
  {
    all.push_back(node);
    parametersUnder(*node, all);
  }

  std::vector<State::Address> res;
  for(auto* node : all)
  {
    if(!node->is<Device::AddressSettings>())
      continue;
    // Audio and texture inputs hold no value, and are recorded too
    auto addr = Device::address(*node).address;
    if(std::find(res.begin(), res.end(), addr) == res.end())
      res.push_back(std::move(addr));
  }
  return res;
}
}
