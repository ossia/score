// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Remove.hpp"

#include "Add/LoadDevice.hpp"
#include "Remove/RemoveAddress.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <QByteArray>

namespace Explorer
{
class DeviceDocumentPlugin;
namespace Command
{
Remove::Remove(const DeviceDocumentPlugin& devplug, Device::NodePath&& path)
    : m_device{false}, m_cmd{new RemoveAddress{devplug, std::move(path)}}
{
}

Remove::Remove(const DeviceDocumentPlugin& devplug, const Device::Node& node)
    : m_device{true}, m_cmd{new LoadDevice{devplug, Device::Node{node}}}
{
}

Remove::~Remove()
{
  delete m_cmd;
}

void Remove::undo(const score::DocumentContext& ctx) const
{
  m_device ? m_cmd->redo(ctx) : m_cmd->undo(ctx);
}

void Remove::redo(const score::DocumentContext& ctx) const
{
  m_device ? m_cmd->undo(ctx) : m_cmd->redo(ctx);
}

void Remove::serializeImpl(DataStreamInput& d) const
{
  d << m_device << m_cmd->serialize();
}

void Remove::deserializeImpl(DataStreamOutput& d)
{
  QByteArray cmd_data;
  d >> m_device >> cmd_data;

  if (m_device)
  {
    m_cmd = new LoadDevice;
  }
  else
  {
    m_cmd = new RemoveAddress;
  }

  m_cmd->deserialize(cmd_data);
}
}
}
