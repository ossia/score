// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "NodeModel.hpp"

#include <ossia/network/value/value_traits.hpp>
#include <State/ValueConversion.hpp>
#include <Device/ItemModels/NodeDisplayMethods.hpp>
namespace RemoteUI
{

NodeModel::NodeModel(QObject* parent) : QAbstractItemModel(parent)
{
}

void NodeModel::add_device(Device::Node n)
{
  beginResetModel();
  m_root.push_back(std::move(n));
  endResetModel();
}
void NodeModel::remove_device(QString n)
{
  beginResetModel();
  auto it = ossia::find_if(
        m_root.children(),
        [&] (const auto& cld) { return cld.displayName() == n; });

  if(it != m_root.end())
    m_root.erase(it);
  endResetModel();
}

QString NodeModel::nodeToAddressString(QModelIndex idx)
{
  if (!idx.isValid())
    return {};
  auto& n = nodeFromModelIndex(idx);
  return Device::address(n).toString();
}

Device::Node& NodeModel::rootNode()
{
  return m_root;
}

const Device::Node& NodeModel::rootNode() const
{
  return m_root;
}

QVariant
NodeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  return {};
}

int NodeModel::columnCount(const QModelIndex& parent) const
{
  return 2;
}

Qt::ItemFlags NodeModel::flags(const QModelIndex& index) const
{
  Qt::ItemFlags f = Qt::ItemIsEnabled;
  f |= Qt::ItemIsSelectable;
  f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
  return f;
}

QVariant NodeModel::data(const QModelIndex& index, int role) const
{
  const Device::Node& n = nodeFromModelIndex(index);

  switch (role)
  {
    case Roles::Name:
      return n.displayName();

    case Roles::Value:
    {
      if (auto addr = n.target<Device::AddressSettings>())
      {
        const ossia::value& val = addr->value;
        if (ossia::is_array(val))
        {
          return State::convert::toPrettyString(val);
        }
        else
        {
          return State::convert::value<QVariant>(val);
        }
      }
      else
      {
        return QVariant();
      }
    }

    default:
      return QVariant();
  }
}

QHash<int, QByteArray> NodeModel::roleNames() const
{
  return {{Roles::Name, "address"}, {Roles::Value, "value"}};
}

Device::Node& NodeModel::nodeFromModelIndex(const QModelIndex& index) const
{
  auto n = index.isValid() ? static_cast<NodeType*>(index.internalPointer())
                           : const_cast<NodeType*>(&rootNode());

  SCORE_ASSERT(n);
  return *n;
}

QModelIndex NodeModel::parent(const QModelIndex& index) const
{
  if (!index.isValid())
    return QModelIndex();
  if (index.model() != this)
    return QModelIndex();

  const auto& node = nodeFromModelIndex(index);
  auto parentNode = node.parent();

  if (!parentNode)
    return QModelIndex();

  auto grandparentNode = parentNode->parent();

  if (!grandparentNode)
    return QModelIndex();

  const int rowParent = grandparentNode->indexOfChild(parentNode);
  if (rowParent == -1)
    return QModelIndex();

  return createIndex(rowParent, 0, parentNode);
}

QModelIndex
NodeModel::index(int row, int column, const QModelIndex& parent) const
{
  auto& p = nodeFromModelIndex(parent);
  qDebug() << p.displayName() << rowCount(parent) << columnCount(parent);

  if (!hasIndex(row, column, parent))
    return QModelIndex();

  auto& parentItem = nodeFromModelIndex(parent);

  if (parentItem.hasChild(row))
    return createIndex(row, column, &parentItem.childAt(row));
  else
    return QModelIndex();
}

int NodeModel::rowCount(const QModelIndex& parent) const
{
  if (parent.column() > 0)
    return 0;

  const auto& parentNode = nodeFromModelIndex(parent);

  return parentNode.childCount();
}

bool NodeModel::hasChildren(const QModelIndex& parent) const
{
  const auto& parentNode = nodeFromModelIndex(parent);
  return parentNode.childCount() > 0;
}
}
