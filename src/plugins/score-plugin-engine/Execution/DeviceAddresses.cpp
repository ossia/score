#include "DeviceAddresses.hpp"

#include <ossia/dataflow/for_each_port.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>
// destination_t holds a traversal::path alternative: the variant needs it complete
#include <ossia/network/common/path.hpp>

#include <vector>

namespace Execution
{

ossia::hash_set<const void*> deviceAddresses(ossia::net::device_base& d)
{
  ossia::hash_set<const void*> owned;

  // children_copy() rather than children(): the tree lock is taken per node,
  // so nothing is held while descending.
  std::vector<ossia::net::node_base*> stack{&d.get_root_node()};
  while(!stack.empty())
  {
    auto* n = stack.back();
    stack.pop_back();

    owned.insert(n);
    if(auto* p = n->get_parameter())
      owned.insert(p);

    for(auto* c : n->children_copy())
      stack.push_back(c);
  }

  return owned;
}

bool addressBelongsTo(
    const ossia::destination_t& dest, const ossia::hash_set<const void*>& owned) noexcept
{
  if(auto p = dest.target<ossia::net::parameter_base*>())
    return *p && owned.contains(*p);
  if(auto n = dest.target<ossia::net::node_base*>())
    return *n && owned.contains(*n);
  return false;
}

void clearAddresses(
    std::span<ossia::graph_node* const> nodes,
    const ossia::hash_set<const void*>& owned) noexcept
{
  for(auto node : nodes)
  {
    ossia::for_each_inlet(*node, [&](ossia::inlet& p) {
      if(addressBelongsTo(p.address, owned))
        p.address = {};
    });
    ossia::for_each_outlet(*node, [&](ossia::outlet& p) {
      if(addressBelongsTo(p.address, owned))
        p.address = {};
    });
  }
}
}
