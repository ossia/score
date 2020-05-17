// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ReplaceDevice.hpp"

#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <vector>

namespace Explorer
{
namespace Command
{
// TODO fix this to use NodeUpdateProxy. Maybe it should be a Remove() followed
// by
// a LoadDevice() ?
ReplaceDevice::ReplaceDevice(
    const DeviceDocumentPlugin& device_tree,
    int deviceIndex,
    Device::Node&& rootNode)
    : m_deviceIndex(deviceIndex), m_deviceNode{std::move(rootNode)}
{
  auto& explorer = device_tree.explorer();
  m_savedNode = explorer.nodeFromModelIndex(explorer.index(m_deviceIndex, 0, QModelIndex()));
}

ReplaceDevice::ReplaceDevice(
    const DeviceDocumentPlugin& device_tree,
    int deviceIndex,
    Device::Node&& oldRootNode,
    Device::Node&& newRootNode)
    : m_deviceIndex(deviceIndex)
    , m_deviceNode{std::move(newRootNode)}
    , m_savedNode{std::move(oldRootNode)}
{
}

static void replaceDevice(const Device::Node& new_d, const score::DocumentContext& ctx)
{
  auto& explorer = ctx.plugin<DeviceDocumentPlugin>().explorer();

  const auto& cld = explorer.rootNode().children();
  for (auto it = cld.begin(); it != cld.end(); ++it)
  {
    auto ds = it->get<Device::DeviceSettings>();
    if (ds.name == new_d.get<Device::DeviceSettings>().name)
    {
      explorer.removeNode(it);
      break;
    }
  }

  explorer.addDevice(new_d);
}
void ReplaceDevice::undo(const score::DocumentContext& ctx) const
{
  replaceDevice(m_savedNode, ctx);
}

void ReplaceDevice::redo(const score::DocumentContext& ctx) const
{
  replaceDevice(m_deviceNode, ctx);
}

void ReplaceDevice::serializeImpl(DataStreamInput& d) const
{
  d << m_deviceIndex << m_deviceNode << m_savedNode;
}

void ReplaceDevice::deserializeImpl(DataStreamOutput& d)
{
  d >> m_deviceIndex >> m_deviceNode >> m_savedNode;
}
}
}
