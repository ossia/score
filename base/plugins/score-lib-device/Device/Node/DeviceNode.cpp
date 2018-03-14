// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QByteArray>
#include <QDebug>
#include <eggs/variant/variant.hpp>
#include <vector>

#include "DeviceNode.hpp"
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <score/tools/Todo.hpp>
#include <score/model/tree/TreeNode.hpp>

template class SCORE_LIB_DEVICE_EXPORT boost::container::stable_vector<Device::Node>;
template class SCORE_LIB_DEVICE_EXPORT TreeNode<Device::DeviceExplorerNode>;
namespace Device
{
bool operator<(const Device::Node& lhs, const Device::Node& rhs)
{
  return false;
}
const QString& DeviceExplorerNode::displayName() const
{
  struct
  {
  public:
    using return_type = const QString&;
    return_type operator()(const Device::DeviceSettings& dev) const
    {
      return dev.name;
    }

    return_type operator()(const Device::AddressSettings& addr) const
    {
      return addr.name;
    }

    return_type operator()(InvisibleRootNode) const
    {
      static QString s{"Invisible Root DeviceExplorerNode"};
      return s;
    }
  } visitor{};

  return eggs::variants::apply(visitor, impl());
}

bool DeviceExplorerNode::isSelectable() const
{
  return true;
}

bool DeviceExplorerNode::isEditable() const
{
  return is<Device::AddressSettings>()
         && hasOutput(get<Device::AddressSettings>().ioType);
}

Device::Node* getNodeFromString(Device::Node& n, const QStringList& parts)
{
  auto theN = try_getNodeFromString(n, parts);
  SCORE_ASSERT(theN);
  return theN;
}

Device::Node& getNodeFromAddress(Device::Node& n, const State::Address& addr)
{
  auto theN = try_getNodeFromAddress(n, addr);
  SCORE_ASSERT(theN);
  return *theN;
}

void address_rec(QStringList& path, const Node* n, const Device::Node*& root)
{
  if(n->parent() && !n->is<DeviceSettings>())
  {
    address_rec(path, n->parent(), root);
    path.push_back(n->get<AddressSettings>().name);
  }
  else
  {
    root = n;
  }
}

State::AddressAccessor address(const Node& treeNode)
{
  State::AddressAccessor addr;
  const Node* n = &treeNode;

  if (n->is<Device::AddressSettings>())
    addr.qualifiers.get().unit = n->get<Device::AddressSettings>().unit;

  addr.address.path.reserve(8);
  address_rec(addr.address.path, n, n);

  SCORE_ASSERT(n);
  SCORE_ASSERT(n->is<DeviceSettings>());
  addr.address.device = n->get<DeviceSettings>().name;

  return addr;
}

void parametersList(const Node& n, State::MessageList& ml)
{
  if (n.is<AddressSettings>())
  {
    const auto& stgs = n.get<AddressSettings>();

    if (stgs.ioType == ossia::access_mode::BI)
    {
      ml.push_back(message(n));
    }
  }

  for (const auto& child : n.children())
  {
    parametersList(child, ml);
  }
}

State::Message message(const Node& node)
{
  if (!node.is<Device::AddressSettings>())
    return {};

  auto& s = node.get<Device::AddressSettings>();

  State::Message mess;
  mess.address = address(node);
  mess.address.qualifiers.get().unit = s.unit;
  mess.value = s.value;

  return mess;
}

// TESTME
// TODO : this is really a pattern
// (see score2OSSIA, score_plugin_coppa and friends), try to refactor it.
// This could be a try_insert algorithm.
void merge(Device::Node& base, const State::Message& message)
{
  using Device::Node;

  QStringList path = message.address.address.path;
  path.prepend(message.address.address.device);

  ptr<Node> node = &base;
  for (int i = 0; i < path.size(); i++)
  {
    auto it
        = std::find_if(node->begin(), node->end(), [&](const auto& cur_node) {
            return cur_node.displayName() == path[i];
          });

    if (it == node->end())
    {
      // We have to start adding sub-nodes from here.
      ptr<Node> parentnode{node};
      for (int k = i; k < path.size(); k++)
      {
        ptr<Node> newNode;
        if (k == 0)
        {
          // We're adding a device
          Device::DeviceSettings dev;
          dev.name = path[k];
          newNode = &parentnode->emplace_back(std::move(dev), nullptr);
        }
        else
        {
          // We're adding an address
          Device::AddressSettings addr;
          addr.name = path[k];

          if (k == path.size() - 1)
          {
            // End of the address
            addr.value = message.value;

            // Note : since we don't have this
            // information in messagelist's,
            // we assign a default Out value
            // so that we only send the nodes that actually had messages
            // via the OSSIA api.
            addr.ioType = ossia::access_mode::SET;
          }

          newNode = &parentnode->emplace_back(std::move(addr), nullptr);
        }

        // TODO do similar simplification on other similar algorithms
        // cf in ossia stuff
        parentnode = newNode;
      }

      break;
    }
    else
    {
      node = &*it;

      if (i == path.size() - 1)
      {
        // We replace the value by the one in the message
        if (node->is<Device::AddressSettings>())
        {
          node->get<Device::AddressSettings>().value = message.value;
        }
      }
    }
  }
}

Device::Node merge(Device::Node base, const State::MessageList& other)
{
  using namespace score;
  // For each node in other, if we can also find a similar node in
  // base, we replace its data
  // Else, we insert it.

  SCORE_ASSERT(base.is<InvisibleRootNode>());

  for (const auto& message : other)
  {
    merge(base, message);
  }

  return base;
}

void dumpTree(const Node& node, QString rec)
{
  qDebug() << rec.toUtf8().constData()
           << node.displayName().toUtf8().constData();
  rec += " ";
  for (const auto& child : node)
  {
    dumpTree(child, rec);
  }
}

QString deviceName(const Node& treeNode)
{
  const Node* n = &treeNode;
  while (n->parent() && !n->is<DeviceSettings>())
  {
    n = n->parent();
  }

  SCORE_ASSERT(n);
  SCORE_ASSERT(n->is<DeviceSettings>());
  return n->get<DeviceSettings>().name;
}
}
