// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AddAddress.hpp"

#include <Device/Node/DeviceNode.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>

#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Explorer
{
namespace Command
{
AddAddress::AddAddress(
    const DeviceDocumentPlugin& devplug,
    const Device::NodePath& nodePath,
    InsertMode insert,
    const Device::AddressSettings& addressSettings)
{
  m_addressSettings = addressSettings;

  const Device::Node* parentNode{};

  // DeviceExplorerWidget prevents adding a sibling on a Device
  switch (insert)
  {
    case InsertMode::AsChild:
      parentNode = nodePath.toNode(&devplug.rootNode());
      break;
    case InsertMode::AsSibling:
      parentNode = nodePath.toNode(&devplug.rootNode())->parent();
      break;
    default:
      throw std::runtime_error("AddAddress: Invalid InsertMode");
  }

  SCORE_ASSERT(parentNode);
  m_parentNodePath = Device::NodePath{*parentNode};

  int i = 0;
  if(m_addressSettings.name.contains('/'))
  {
    auto split = m_addressSettings.name.split('/');

    const Device::Node* lastExistingNode = parentNode;
    for(; i < split.size(); i++)
    {
      auto cld = Device::findChildNode(*lastExistingNode, split[i]);
      if(cld)
      {
        lastExistingNode = cld;
        continue;
      }
      else
      {
        break;
      }
    }
    m_createdLevels = split.size() - i;
  }
  else
  {
    m_createdLevels = 1;
  }
}

void AddAddress::undo(const score::DocumentContext& ctx) const
{
  auto& devplug = ctx.plugin<DeviceDocumentPlugin>();

  devplug.updateProxy.removeNode(m_parentNodePath, m_addressSettings);
  if(m_createdLevels > 1)
  {
    auto split = m_addressSettings.name.split('/');
    auto set = m_addressSettings;
    int levels = m_createdLevels - 1;
    while(levels > 0)
    {
      split.pop_back();
      set.name = split.join('/');
      devplug.updateProxy.removeNode(m_parentNodePath, set);
      levels--;
    }
  }
}

void AddAddress::redo(const score::DocumentContext& ctx) const
{
  auto& devplug = ctx.plugin<DeviceDocumentPlugin>();
  devplug.updateProxy.addAddress(m_parentNodePath, m_addressSettings, 0);
}

void AddAddress::serializeImpl(DataStreamInput& s) const
{
  s << m_parentNodePath << m_addressSettings << m_createdLevels;
}

void AddAddress::deserializeImpl(DataStreamOutput& s)
{
  s >> m_parentNodePath >> m_addressSettings >> m_createdLevels;
}

AddWholeAddress::AddWholeAddress(
    const DeviceDocumentPlugin& devplug,
    const Device::FullAddressSettings& addr)
{
  m_addressSettings = addr;

  if (devplug.list().findDevice(addr.address.device))
  {
    // TODO
    /*
    auto iter = &devplug.rootNode();
        auto find_cld = [&] (QString toFind) {
          return [&, toFind] (auto& cld) {
            if(cld.prettyName() == toFind)
            {
              iter = &cld;
              return true;
            }
            else
            {
              return false;
            }
          };
        };
        if(!iter->visit(find_cld(addr.address.device)))
        {
          throw std::runtime_error("Non-existing device");
          return;
        }

        // at this point iter is the device node
        const int pathSize = addr.address.path.size();
        for (int i = 0; i < pathSize; ++i)
        {
          if(iter->visit(find_cld(addr.address.path[i])))
          {
            continue;
          }
          else
          {
            m_existsUpTo = i;
            break;
          }
        }
        */
  }
}

void AddWholeAddress::undo(const score::DocumentContext& ctx) const
{
  // TODO
  // auto& devplug = ctx.plugin<DeviceDocumentPlugin>();
  // devplug.updateProxy.removeNode(m_parentNodePath, m_addressSettings);
}

void AddWholeAddress::redo(const score::DocumentContext& ctx) const
{
  auto& devplug = ctx.plugin<DeviceDocumentPlugin>();
  devplug.updateProxy.addAddress(m_addressSettings);
}

void AddWholeAddress::serializeImpl(DataStreamInput& s) const
{
  s << m_addressSettings << m_existsUpTo;
}

void AddWholeAddress::deserializeImpl(DataStreamOutput& s)
{
  s >> m_addressSettings >> m_existsUpTo;
}
}
}
