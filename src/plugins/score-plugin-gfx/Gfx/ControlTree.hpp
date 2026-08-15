#pragma once

/**
 * @file ControlTree.hpp
 * @brief Turn a list of control descriptions into device-tree nodes.
 *
 * Two very different things end up as settings under a video device: what the
 * driver publishes (gain, exposure, white balance -- discovered at runtime and
 * different on every camera) and what score itself offers (scale mode, and the
 * demosaic's own corrections). They share no vocabulary, so this takes the one
 * thing they do have in common -- a name, a type, a domain and something to do
 * on write -- and builds the nodes from that.
 *
 * Kept free of V4L2 so the score-side group is not obliged to invent a fake
 * driver control to describe itself.
 */

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Gfx
{

/// One settable thing, described independently of where it came from.
struct TreeControl
{
  std::string name;        ///< tree-safe; becomes the address component
  std::string description; ///< shown in the explorer

  ossia::val_type type{};
  ossia::domain domain;
  ossia::value initial;
  ossia::access_mode access{ossia::access_mode::BI};

  /// Called when something writes the parameter, on the writing thread.
  /// Empty for a read-only value that is only ever pushed outward.
  std::function<void(const ossia::value&)> onSet;
};

/**
 * @brief Creates `<parent>/<group>/<name>` for each control.
 *
 * @returns the created parameters, in the order of @p controls, so the caller
 * can push values back into them later. An entry is null when its node could
 * not be created -- a name collision, in practice.
 */
inline std::vector<ossia::net::parameter_base*> addControlGroup(
    ossia::net::device_base& dev, ossia::net::node_base& parent,
    const std::string& group, const std::vector<TreeControl>& controls)
{
  std::vector<ossia::net::parameter_base*> out;
  out.reserve(controls.size());

  auto groupNode = std::make_unique<ossia::net::generic_node>(group, dev, parent);
  auto* groupPtr = parent.add_child(std::move(groupNode));
  if(!groupPtr)
  {
    out.resize(controls.size(), nullptr);
    return out;
  }

  for(const auto& c : controls)
  {
    auto node = std::make_unique<ossia::net::generic_node>(c.name, dev, *groupPtr);
    auto* param = node->create_parameter(c.type);
    if(!param)
    {
      out.push_back(nullptr);
      continue;
    }

    if(c.domain)
      param->set_domain(c.domain);
    param->set_access(c.access);

    if(!c.description.empty())
      ossia::net::set_description(*node, c.description);

    // The initial value is set before the callback is installed: it describes
    // what the hardware already holds, and writing it back would be a
    // round-trip through the driver for no reason -- and, for a control whose
    // write performs an action, an unwanted action.
    if(c.initial.valid())
      param->push_value(c.initial);

    if(c.onSet)
    {
      auto cb = c.onSet;
      param->add_callback(std::move(cb));
    }

    // add_child takes ownership; the node keeps the parameter alive.
    auto* added = groupPtr->add_child(std::move(node));
    out.push_back(added ? param : nullptr);
  }

  return out;
}

} // namespace Gfx
