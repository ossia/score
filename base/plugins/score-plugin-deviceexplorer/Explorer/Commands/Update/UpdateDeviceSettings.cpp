// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <algorithm>
#include <vector>

#include "UpdateDeviceSettings.hpp"
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/tree/TreeNode.hpp>

namespace Explorer
{
namespace Command
{
UpdateDeviceSettings::UpdateDeviceSettings(
    const DeviceDocumentPlugin& devplug,
    const QString& name,
    const Device::DeviceSettings& parameters)
    : m_newParameters(parameters)
{
  auto it = std::find_if(
      devplug.rootNode().begin(),
      devplug.rootNode().end(),
      [&](const Device::Node& n) {
        return n.get<Device::DeviceSettings>().name == name;
      });

  SCORE_ASSERT(it != devplug.rootNode().end());

  m_oldParameters = (*it).get<Device::DeviceSettings>();
}

void UpdateDeviceSettings::undo(const score::DocumentContext& ctx) const
{
  auto& devplug = ctx.plugin<DeviceDocumentPlugin>();
  devplug.updateProxy.updateDevice(m_newParameters.name, m_oldParameters);
}

void UpdateDeviceSettings::redo(const score::DocumentContext& ctx) const
{
  auto& devplug = ctx.plugin<DeviceDocumentPlugin>();
  devplug.updateProxy.updateDevice(m_oldParameters.name, m_newParameters);
}

void UpdateDeviceSettings::serializeImpl(DataStreamInput& d) const
{
  d << m_oldParameters << m_newParameters;
}

void UpdateDeviceSettings::deserializeImpl(DataStreamOutput& d)
{
  d >> m_oldParameters >> m_newParameters;
}
}
}
