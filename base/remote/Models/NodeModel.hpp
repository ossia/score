#ifndef NODEMODEL_H
#define NODEMODEL_H

#include <QAbstractItemModel>
#include <Device/Node/DeviceNode.hpp>
#include <unordered_map>

namespace RemoteUI
{

enum Roles : int
{
  Name = Qt::UserRole + 10,
  Value,

  Count // column count, always last
};

//! Data model for the device tree
class NodeModel : public QAbstractItemModel
{
  Q_OBJECT

public:
  explicit NodeModel(QObject* parent = 0);

  void add_device(Device::Node n);
  void remove_device(QString n);
  Q_INVOKABLE QString nodeToAddressString(QModelIndex idx);
  Device::Node& rootNode();
  const Device::Node& rootNode() const;

private:
  using NodeType = Device::Node;
  QVariant headerData(
      int section,
      Qt::Orientation orientation,
      int role = Qt::DisplayRole) const override;

  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QVariant
  data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

  QHash<int, QByteArray> roleNames() const override;

  Device::Node& nodeFromModelIndex(const QModelIndex& index) const;

  QModelIndex parent(const QModelIndex& index) const final override;

  QModelIndex
  index(int row, int column, const QModelIndex& parent) const final override;

  int rowCount(const QModelIndex& parent) const final override;

  bool hasChildren(const QModelIndex& parent) const final override;
  Device::Node m_root;
};
}

#endif // NODEMODEL_H
