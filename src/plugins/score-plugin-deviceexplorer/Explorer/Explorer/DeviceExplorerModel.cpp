// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceExplorerModel.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/ItemModels/NodeBasedItemModel.hpp>
#include <Device/ItemModels/NodeDisplayMethods.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Node/NodeListMimeSerialization.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/Commands/Update/UpdateAddressSettings.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Explorer/DeviceExplorerMimeTypes.hpp>
#include <Explorer/Explorer/DeviceExplorerView.hpp>
#include <Explorer/Explorer/Widgets/DeviceEditDialog.hpp> // TODO why here??!!
#include <State/MessageListSerialization.hpp>
#include <State/ValueConversion.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/CommandStackFacade.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/MimeVisitor.hpp>

#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/domain/domain.hpp>

#include <QAbstractProxyModel>
#include <QApplication>
#include <QMap>
#include <QMimeData>
#include <QObject>
#include <QTreeView>
#include <qtypetraits.h>

#include <wobjectimpl.h>

#include <iostream>
#include <string>
#include <vector>
W_OBJECT_IMPL(Explorer::DeviceExplorerModel)
namespace Explorer
{
static const QMap<Explorer::Column, QString> HEADERS{
    {Explorer::Column::Name, QObject::tr("Address")},
    {Explorer::Column::Value, QObject::tr("Value")}};

DeviceExplorerModel::DeviceExplorerModel(DeviceDocumentPlugin& plug, QObject* parent)
    : NodeBasedItemModel{parent}
    , m_devicePlugin{plug}
    , m_rootNode{plug.rootNode()}
    , m_cmdQ{plug.context().commandStack}
{
  this->setObjectName("DeviceExplorerModel");

  beginResetModel();
  endResetModel();
}

DeviceExplorerModel::~DeviceExplorerModel() { }

DeviceDocumentPlugin& DeviceExplorerModel::deviceModel() const
{
  return m_devicePlugin;
}

QModelIndexList DeviceExplorerModel::selectedIndexes() const
{
  if (!m_view)
  {
    return {};
  }
  else
  {
    // We have to do this check if we have a proxy
    if (m_view->hasProxy())
    {
      auto indexes = m_view->selectedIndexes();
      for (auto& index : indexes)
        index = static_cast<const QAbstractProxyModel*>(m_view->QTreeView::model())
                    ->mapToSource(index);
      return indexes;
    }
    else
    {
      return m_view->selectedIndexes();
    }
  }
}

QStringList DeviceExplorerModel::getColumns() const
{
  return HEADERS.values();
}

int DeviceExplorerModel::addDevice(const Device::Node& deviceNode)
{
  int row = m_rootNode.childCount();
  QModelIndex parent; // invalid

  beginInsertRows(parent, row, row);
  rootNode().push_back(deviceNode);
  endInsertRows();

  return row;
}

int DeviceExplorerModel::addDevice(Device::Node&& deviceNode)
{
  deviceNode.setParent(&rootNode());

  int row = m_rootNode.childCount();
  QModelIndex parent; // invalid

  beginInsertRows(parent, row, row);
  rootNode().push_back(std::move(deviceNode));
  endInsertRows();

  return row;
}

void DeviceExplorerModel::updateDevice(const QString& name, const Device::DeviceSettings& dev)
{
  int i = 0;
  for (auto& n : m_rootNode)
  {
    if (n.get<Device::DeviceSettings>().name == name)
    {
      n.set(dev);

      QModelIndex index = createIndex(i, 0, n.parent());
      dataChanged(index, index);
      return;
    }
    i++;
  }
}

void DeviceExplorerModel::addAddress(
    Device::Node* parentNode,
    const Device::AddressSettings& addressSettings,
    int row)
{
  SCORE_ASSERT(parentNode);
  SCORE_ASSERT(parentNode != &m_rootNode);

  Device::Node* grandparent = parentNode->parent();
  SCORE_ASSERT(grandparent);
  int rowParent = grandparent->indexOfChild(parentNode);
  QModelIndex parentIndex = createIndex(rowParent, 0, parentNode);

  beginInsertRows(parentIndex, row, row);

  auto it = parentNode->begin();
  std::advance(it, row);

  parentNode->emplace(it, addressSettings, parentNode);

  endInsertRows();
}

void DeviceExplorerModel::addNode(Device::Node* parentNode, Device::Node&& child, int row)
{
  SCORE_ASSERT(parentNode);
  SCORE_ASSERT(parentNode != &m_rootNode);

  Device::Node* grandparent = parentNode->parent();
  SCORE_ASSERT(grandparent);
  int rowParent = grandparent->indexOfChild(parentNode);
  QModelIndex parentIndex = createIndex(rowParent, 0, parentNode);

  beginInsertRows(parentIndex, row, row);

  auto it = parentNode->begin();
  std::advance(it, row);
  parentNode->emplace(it, std::move(child));

  endInsertRows();
}

void DeviceExplorerModel::updateAddress(
    Device::Node* node,
    const Device::AddressSettings& addressSettings)
{
  SCORE_ASSERT(node);
  SCORE_ASSERT(node != &m_rootNode);

  node->set(addressSettings);

  nodeChanged(node);

  dataChanged(modelIndexFromNode(*node, 0), modelIndexFromNode(*node, (int)Column::Count - 1));
}

void DeviceExplorerModel::updateValue(
    Device::Node* n,
    const State::AddressAccessor& addr,
    const ossia::value& v)
{
  if (!addr.qualifiers.get().accessors.empty())
  {
    SCORE_TODO;
  }
  n->get<Device::AddressSettings>().value = v;

  QModelIndex nodeIndex = modelIndexFromNode(*n, 1);

  dataChanged(nodeIndex, nodeIndex);
}

bool DeviceExplorerModel::checkDeviceInstantiatable(Device::DeviceSettings& n)
{
  // No name -> no love
  if (n.name.isEmpty())
    return false;

  // Request from the protocol factory the protocol to see
  // if it is compatible.
  auto& context = m_devicePlugin.context().app;
  auto prot = context.interfaces<Device::ProtocolFactoryList>().get(n.protocol);
  if (!prot)
    return false;

  // Look for other childs in the same protocol.
  return std::none_of(rootNode().begin(), rootNode().end(), [&](const Device::Node& child) {
    SCORE_ASSERT(child.is<Device::DeviceSettings>());
    const auto& set = child.get<Device::DeviceSettings>();
    return (set.name == n.name)
           || (set.protocol == n.protocol
               && !prot->checkCompatibility(n, child.get<Device::DeviceSettings>()));
  });
}

bool DeviceExplorerModel::tryDeviceInstantiation(
    Device::DeviceSettings& set,
    DeviceEditDialog& dial)
{
  while (!checkDeviceInstantiatable(set))
  {
#if defined(__EMSCRIPTEN__)
    return false;
#else
    dial.setSettings(set);
    dial.setEditingInvalidState(true);

    bool ret = dial.exec();
    if (!ret)
    {
      dial.setEditingInvalidState(false);
      return false;
    }

    set = dial.getSettings();
#endif
  }

  dial.setEditingInvalidState(true);
  return true;
}

bool DeviceExplorerModel::checkAddressInstantiatable(
    Device::Node& parent,
    const Device::AddressSettings& addr)
{
  SCORE_ASSERT(!parent.is<InvisibleRootNode>());

  if (addr.name.isEmpty())
    return false;

  return std::none_of(parent.begin(), parent.end(), [&](const Device::Node& n) {
    return n.get<Device::AddressSettings>().name == addr.name;
  });
}

bool DeviceExplorerModel::checkAddressEditable(
    Device::Node& parent,
    const Device::AddressSettings& before,
    const Device::AddressSettings& after)
{
  SCORE_ASSERT(!parent.is<InvisibleRootNode>());

  if (after.name.isEmpty())
    return false;

  auto it = std::find_if(parent.begin(), parent.end(), [&](const Device::Node& n) {
    return n.get<Device::AddressSettings>().name == after.name;
  });
  if (it != parent.end())
  {
    //  We didn't change name, it's ok
    if (after.name == before.name)
      return true;
    else
      return false;
  }
  else
  {
    // Ok, no conflicts
    return true;
  }
}

int DeviceExplorerModel::columnCount() const
{
  return (int)Column::Count;
}

int DeviceExplorerModel::columnCount(const QModelIndex& /*parent*/) const
{
  return (int)Column::Count;
}

// must return an invalid QVariant for cases not handled
QVariant DeviceExplorerModel::data(const QModelIndex& index, int role) const
{
  const int col = index.column();

  if (col < 0 || col >= (int)Column::Count)
  {
    return QVariant();
  }

  const Device::Node& n = nodeFromModelIndex(index);
  if (role != Qt::ToolTipRole)
  {
    switch ((Column)col)
    {
      case Column::Name:
      {
        if (n.is<Device::AddressSettings>())
          return Device::nameColumnData(n, role);
        else if (n.is<Device::DeviceSettings>())
        {
          auto& dev_set = n.get<Device::DeviceSettings>();
          return Device::deviceNameColumnData(
              n, deviceModel().list().device(dev_set.name).connected(), role);
        }
        return {};
      }

      case Column::Value:
        return Device::valueColumnData(n, role);

      case Column::Count:
      default:
        SCORE_ABORT;
        return {};
    }
  }
  else
  {
    // Tooltip
    if (n.is<Device::AddressSettings>())
    {
      auto& addr_set = n.get<Device::AddressSettings>();
      if (const auto& desc = ossia::net::get_description(addr_set))
        return QString::fromStdString(*desc);
    }
    else
    {
      return {};
    }
  }
  return {};
}

QVariant DeviceExplorerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
  {
    if (section >= 0 && section < (int)Column::Count)
    {
      return HEADERS[(Column)section];
    }
  }

  return {};
}

Qt::ItemFlags DeviceExplorerModel::flags(const QModelIndex& index) const
{
  Qt::ItemFlags f = Qt::ItemIsEnabled;
  // by default QAbstractItemModel::flags(index); returns Qt::ItemIsEnabled |
  // Qt::ItemIsSelectable

  if (index.isValid())
  {
    const Device::Node& n = nodeFromModelIndex(index);

    if (n.isSelectable())
    {
      f |= Qt::ItemIsSelectable;
    }

    // we allow drag'n drop only from the name column
    if (index.column() == (int)Column::Name)
    {
      f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    }

    if (n.isEditable() && index.column() == (int)Column::Value)
    {
      f |= Qt::ItemIsEditable;
    }
  }
  else
  {
    // to be able to drop even where there is nothing
    f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
  }

  return f;
}

/*
  return false if no change was made.
  dataChanged() & return true if a change is made.

  Note: this is the function that gets called when the user changes the value
  in the tree.
  It then sends a command that calls editData.
*/
bool DeviceExplorerModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if (!index.isValid())
    return false;

  auto& n = nodeFromModelIndex(index);

  if (!n.is<Device::AddressSettings>())
    return false;

  auto col = Explorer::Column(index.column());

  if (role == Qt::EditRole || role == Qt::CheckStateRole)
  {
    if (col == Column::Value)
    {
      // In this case we don't make a command, but we directly push the
      // new value.
      auto copy = State::convert::fromQVariant(value);

      // We may have to convert types.
      const ossia::value& orig = n.get<Device::AddressSettings>().value;
      if (copy.v.which() != orig.v.which() && !State::convert::convert(orig, copy))
        return false;

      n.get<Device::AddressSettings>().value = copy;

      // Note : if we want to disable remote updating, we have to do it
      // here (e.g. if this becomes a settings)
      m_devicePlugin.updateProxy.updateRemoteValue(Device::address(n).address, copy);

      return true;
    }
    else
    {
      // Here we make a command because we change the structure of the tree.
      auto settings = n.get<Device::AddressSettings>();
      if (col == Column::Name)
      {
        const QString s = value.toString();
        if (!s.isEmpty())
        {
          settings.name = s;
        }
      }

      if (settings != n.get<Device::AddressSettings>())
      {
        // We changed
        m_cmdQ.redoAndPush(new Explorer::Command::UpdateAddressSettings{
            this->deviceModel(), Device::NodePath{n}, settings});
        return true;
      }
    }
  }

  return false;
}

bool DeviceExplorerModel::setHeaderData(int, Qt::Orientation, const QVariant&, int)
{
  return false; // we prevent editing the (column) headers
}

/**
 * @brief DeviceExplorerModel::editData
 *
 * This functions gets called by the command
 * that edit the columns.
 */
void DeviceExplorerModel::editData(
    const Device::NodePath& path,
    Explorer::Column column,
    const ossia::value& value,
    int role)
{
  Device::Node* node = path.toNode(&rootNode());
  editData(*node, column, value, role);
}

void DeviceExplorerModel::editData(
    Device::Node& node,
    Column column,
    const ossia::value& value,
    int role)
{
  SCORE_ASSERT(node.parent());

  QModelIndex index = createIndex(node.parent()->indexOfChild(&node), (int)column, node.parent());

  QModelIndex changedTopLeft = index;
  QModelIndex changedBottomRight = index;

  if (node.is<Device::DeviceSettings>())
    return;

  if (role == Qt::EditRole)
  {
    SCORE_TODO;
    /*
        if(index.column() == (int)Column::Name)
        {
            const QString s = value.toString();

            if(! s.isEmpty())
            {
                node.get<Device::AddressSettings>().name = s;
            }
        }
        else */
    if (index.column() == (int)Column::Value)
    {
      node.get<Device::AddressSettings>().value = value;
    }
  }

  dataChanged(changedTopLeft, changedBottomRight);
}

QModelIndex DeviceExplorerModel::bottomIndex(const QModelIndex& index) const
{
  auto& node = nodeFromModelIndex(index);

  if (!node.hasChildren())
  {
    return index;
  }

  return bottomIndex(
      createIndex(node.childCount() - 1, index.column(), &node.childAt(node.childCount() - 1)));
}

bool DeviceExplorerModel::isDevice(QModelIndex index) const
{
  if (!index.isValid())
  {
    return false;
  }

  const Device::Node& n = nodeFromModelIndex(index);
  return n.is<Device::DeviceSettings>();
}

bool DeviceExplorerModel::isEmpty() const
{
  return m_rootNode.childCount() == 0;
}

/*
Drag and drop works by deleting the dragged items and creating a new set of
dropped items that match those dragged.
I will/may call insertRows(), removeRows(), dropMimeData(), ...
We define two MimeTypes : address and device.
It allows to distinguish whether we are drag'n dropping devices or addresses.
 */

Qt::DropActions DeviceExplorerModel::supportedDropActions() const
{
  return (Qt::CopyAction);
}

// Default supportedDragActions() implementation returns
// supportedDropActions().

Qt::DropActions DeviceExplorerModel::supportedDragActions() const
{
  return (Qt::CopyAction);
}

QStringList DeviceExplorerModel::mimeTypes() const
{
  return {score::mime::device(), score::mime::address()};
}

SelectedNodes DeviceExplorerModel::uniqueSelectedNodes(const QModelIndexList& indexes) const
{
  SelectedNodes nodes;
  ossia::transform(indexes, std::back_inserter(nodes.parents), [&](const QModelIndex& idx) {
    return &nodeFromModelIndex(idx);
  });

  ossia::remove_erase(nodes.parents, &m_rootNode);

  nodes.messages.reserve(nodes.messages.size() + nodes.parents.size());
  if (nodes.parents.size() > 1)
  {
    // Filter messages
    for (auto node : nodes.parents)
    {
      if (node->is<Device::AddressSettings>())
      {
        auto& val = node->get<Device::AddressSettings>();
        if (val.ioType == ossia::access_mode::SET)
        {
          nodes.messages.push_back(node);
        }
      }
    }

    // Filter parents
    nodes.parents = filterUniqueParents(nodes.parents);
  }
  else if (nodes.parents.size() == 1)
  {
    Device::Node* node = nodes.parents[0];
    if (node->is<Device::AddressSettings>() && node->childCount() == 0)
    {
      // If a single node is selected we copy it even if it is "get"
      nodes.messages = std::move(nodes.parents);
    }
  }

  return nodes;
}
// method called when a drag is initiated
QMimeData* DeviceExplorerModel::mimeData(const QModelIndexList& indexes) const
{
  auto uniqueNodes = uniqueSelectedNodes(indexes);

  // Handle case of a single device which can have custom mimeData
  if (uniqueNodes.parents.size() == 1 && uniqueNodes.parents[0]->is<Device::DeviceSettings>())
  {
    auto node = uniqueNodes.parents[0];
    if (node->is<Device::DeviceSettings>())
    {
      auto& dev = deviceModel().list().device(node->get<Device::DeviceSettings>().name);
      if (auto d = dev.mimeData())
        return d;
    }
  }

  auto mimeData = new QMimeData;

  // Now we request an update to the device explorer.
  m_devicePlugin.updateProxy.refreshRemoteValues(uniqueNodes.parents);

  // The "MessagesList" part.
  State::MessageList messages;
  for (const auto& node : uniqueNodes.parents)
  {
    Device::parametersList(*node, messages);
  }

  for (const auto& node : uniqueNodes.messages)
  {
    messages += Device::message(*node);
  }

  if (!messages.empty())
  {
    Mime<State::MessageList>::Serializer s{*mimeData};
    s.serialize(messages);
  }

  // The "Nodes" part. Deserialize with FreeNodeList to get the adresses.
  {
    Mime<Device::NodeList>::Serializer s{*mimeData};
    Device::NodeList vec;
    vec.reserve(uniqueNodes.parents.size() + uniqueNodes.messages.size());
    vec.insert(vec.end(), uniqueNodes.parents.begin(), uniqueNodes.parents.end());
    vec.insert(vec.end(), uniqueNodes.messages.begin(), uniqueNodes.messages.end());
    s.serialize(vec);
  }

  if (messages.empty() && uniqueNodes.parents.empty() && uniqueNodes.messages.empty())
  {
    delete mimeData;
    return nullptr;
  }

  return mimeData;
}

bool DeviceExplorerModel::canDropMimeData(
    const QMimeData* mimeData,
    Qt::DropAction action,
    int /*row*/,
    int /*column*/,
    const QModelIndex& parent) const
{
  if (action == Qt::IgnoreAction)
  {
    return true;
  }

  if (action != Qt::MoveAction && action != Qt::CopyAction)
  {
    return false;
  }

  if (!mimeData
      || (!mimeData->hasFormat(score::mime::device())
          && !mimeData->hasFormat(score::mime::address())))
  {
    return false;
  }

  const Device::Node& parentNode = nodeFromModelIndex(parent);

  if (mimeData->hasFormat(score::mime::address()))
  {
    if (&parentNode == &m_rootNode)
    {
      return false;
    }
  }
  else
  {
    SCORE_ASSERT(mimeData->hasFormat(score::mime::device()));

    if (&parentNode != &m_rootNode)
    {
      return false;
    }
  }

  return true;
}

// method called when a drop occurs
// return true if drop really handled, false otherwise.
//
// if dropMimeData returns true && action==Qt::MoveAction, removeRows is called
// immediately after
bool DeviceExplorerModel::dropMimeData(
    const QMimeData* mimeData,
    Qt::DropAction action,
    int row,
    int column,
    const QModelIndex& parent)
{
  if (action == Qt::IgnoreAction)
  {
    return true;
  }

  if (action != Qt::MoveAction && action != Qt::CopyAction)
  {
    return false;
  }

  if (!mimeData
      || (!mimeData->hasFormat(score::mime::device())
          && !mimeData->hasFormat(score::mime::address())))
  {
    return false;
  }

  QModelIndex parentIndex; // invalid
  Device::Node* parentNode = &m_rootNode;
  QString mimeType = score::mime::device();

  if (mimeData->hasFormat(score::mime::address()))
  {
    parentIndex = parent;
    parentNode = &nodeFromModelIndex(parent);
    mimeType = score::mime::address();

    if (parentNode == &m_rootNode)
    {
      return false;
    }
  }
  else
  {
    SCORE_ASSERT(mimeData->hasFormat(score::mime::device()));
    SCORE_ASSERT(mimeType == score::mime::device());
  }

  if (parentNode)
  {
    // Note : when dropping a device,
    // if there is an existing device that would use the same ports, etc.
    // we have to open a dialog to change the device settings.

    const auto& json = readJson(mimeData->data(mimeType));
    JSONObject::Deserializer deser{json};
    Device::Node n;
    deser.writeTo(n);

    if (mimeType == score::mime::device())
    {
      SCORE_ASSERT(n.is<Device::DeviceSettings>());

      bool deviceOK = checkDeviceInstantiatable(n.get<Device::DeviceSettings>());
      if (!deviceOK)
      {
        // We ask the user to fix the incompatibilities by himself.
        DeviceEditDialog dial{
            m_devicePlugin.context().app.interfaces<Device::ProtocolFactoryList>(),
            QApplication::activeWindow()};
        if (!tryDeviceInstantiation(n.get<Device::DeviceSettings>(), dial))
          return false;
      }

      // Perform the loading
      auto cmd = new Command::LoadDevice{deviceModel(), std::move(n)};

      m_cmdQ.redoAndPush(cmd);
    }

    return true;
  }
  return false;
}

QModelIndex DeviceExplorerModel::convertPathToIndex(const Device::NodePath& path)
{
  QModelIndex iter;
  const int pathSize = path.size();

  for (int i = 0; i < pathSize; ++i)
  {
    iter = index(path.at(i), 0, iter);
  }

  return iter;
}

void DeviceExplorerModel::debug_printPath(const Device::NodePath& path)
{
  const int pathSize = path.size();

  for (int i = 0; i < pathSize; ++i)
  {
    std::cerr << path.at(i) << " ";
  }

  std::cerr << "\n";
}

void DeviceExplorerModel::debug_printIndexes(const QModelIndexList& indexes)
{
  std::cerr << "indexes: " << indexes.size() << " nodes: \n";
  Q_FOREACH (const QModelIndex& index, indexes)
  {
    if (index.isValid())
    {
      std::cerr << " index.row=" << index.row() << " col=" << index.column() << " ";
      Device::Node* n = &nodeFromModelIndex(index);
      std::cerr << " n=" << n << " ";
      Device::Node* parent = n->parent();

      if (n == &m_rootNode)
      {
        std::cerr << " rootNode parent=" << parent << "\n";
      }
      else
      {
        std::cerr << " n->name=" << n->displayName().toStdString();
        std::cerr << " parent=" << parent;
        std::cerr << " parent->name=" << parent->displayName().toStdString() << "\n";
      }
    }
    else
    {
      std::cerr << " invalid index \n";
    }
  }
}

State::MessageList getSelectionSnapshot(DeviceExplorerModel& model)
{
  // Filter
  auto uniqueNodes = model.uniqueSelectedNodes(model.selectedIndexes());

  // Recursive refresh
  model.deviceModel().updateProxy.refreshRemoteValues(uniqueNodes.parents);

  // Conversion
  State::MessageList messages;
  for (const auto& node : uniqueNodes.parents)
  {
    Device::parametersList(*node, messages);
  }
  for (const auto& node : uniqueNodes.messages)
  {
    messages += Device::message(*node);
  }

  return messages;
}

DeviceExplorerModel* try_deviceExplorerFromObject(const QObject& obj)
{
  auto plug = score::IDocument::documentContext(obj).findPlugin<DeviceDocumentPlugin>();
  if (plug)
    return &plug->explorer();
  return nullptr;
}

DeviceExplorerModel& deviceExplorerFromObject(const QObject& obj)
{
  auto expl = try_deviceExplorerFromObject(obj);
  SCORE_ASSERT(expl);
  return *expl;
}

DeviceExplorerModel* try_deviceExplorerFromContext(const score::DocumentContext& ctx)
{
  auto plug = ctx.findPlugin<DeviceDocumentPlugin>();
  if (plug)
    return &plug->explorer();
  return nullptr;
}

DeviceExplorerModel& deviceExplorerFromContext(const score::DocumentContext& ctx)
{
  auto expl = try_deviceExplorerFromContext(ctx);
  SCORE_ASSERT(expl);
  return *expl;
}

Device::FullAddressAccessorSettings makeFullAddressAccessorSettings(
    const State::AddressAccessor& addr,
    const score::DocumentContext& ctx,
    ossia::value min,
    ossia::value max)
{
  // First try to find if there is a matching address
  // in the device explorer
  auto deviceexplorer = Explorer::try_deviceExplorerFromContext(ctx);
  if (deviceexplorer)
  {
    return Device::makeFullAddressAccessorSettings(
        addr, *deviceexplorer, std::move(min), std::move(max));
  }

  // If there is none, build with some default settings
  Device::FullAddressAccessorSettings s;
  s.address = addr;
  s.domain = ossia::make_domain(std::move(min), std::move(max));
  return s;
}
}
