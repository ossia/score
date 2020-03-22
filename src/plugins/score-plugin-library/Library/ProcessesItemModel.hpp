#pragma once
#include <Process/ProcessFactory.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>
#include <score/tools/std/Optional.hpp>
#include <verdigris>
#include <QIcon>
#include <QJsonObject>

namespace score
{
struct GUIApplicationContext;
}

namespace Library
{
struct ProcessData
{
  QString name;
  QIcon icon;
  QJsonObject json;
  Process::ProcessModelFactory::ConcreteKey key;
};

using ProcessNode = TreeNode<ProcessData>;

class ProcessesItemModel : public TreeNodeBasedItemModel<ProcessNode>
{
public:
  using QAbstractItemModel::beginResetModel;
  using QAbstractItemModel::endResetModel;

  ProcessesItemModel(const score::GUIApplicationContext& ctx, QObject* parent);

  QModelIndex find(const Process::ProcessModelFactory::ConcreteKey& k);

  ProcessNode& rootNode() override;
  const ProcessNode& rootNode() const override;

  // Data reading
  int columnCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  QVariant
  headerData(int section, Qt::Orientation orientation, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  // Drag, drop, etc.
  QStringList mimeTypes() const override;
  QMimeData* mimeData(const QModelIndexList& indexes) const override;
  Qt::DropActions supportedDragActions() const override;

private:
  ProcessNode m_root;
};
}

W_REGISTER_ARGTYPE(optional<Library::ProcessData>)
Q_DECLARE_METATYPE(optional<Library::ProcessData>)
