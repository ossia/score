#pragma once
#include <Library/LibraryInterface.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/std/StringHash.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QAbstractItemModel>
#include <QIcon>
#include <QMimeData>
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
  ProcessNode m_root;

public:
  using QAbstractItemModel::beginResetModel;
  using QAbstractItemModel::endResetModel;

  ProcessesItemModel(const score::GUIApplicationContext& ctx, QObject* parent)
      : TreeNodeBasedItemModel<ProcessNode>{parent}
  {
    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    std::map<QString, std::vector<Process::ProcessModelFactory*>> sorted;
    for (Process::ProcessModelFactory& proc : procs)
    {
      sorted[proc.category()].push_back(&proc);
    }

    for (auto& e : sorted)
    {
      auto& cat = m_root.emplace_back(
          ProcessData{e.first, QIcon{}, QJsonObject{}, {}}, &m_root);
      for (auto p : e.second)
      {
        QJsonObject obj;
        obj["Type"] = "Process";
        obj["uuid"] = toJsonValue(p->concreteKey().impl());
        cat.emplace_back(
            ProcessData{p->prettyName(), QIcon{}, obj, p->concreteKey()},
            &cat);
      }
    }

    auto& lib_setup = ctx.interfaces<Library::LibraryInterfaceList>();
    for (auto& lib : lib_setup)
    {
      lib.setup(*this, ctx);
    }
  }

  QModelIndex find(const Process::ProcessModelFactory::ConcreteKey& k)
  {
    for (auto& cat : m_root)
    {
      auto proc_it = cat.begin();
      for (int i = 0; i < cat.childCount(); ++proc_it, ++i)
      {
        auto& proc = *proc_it;
        if (proc.key == k)
        {
          return createIndex(i, 0, &proc);
        }
      }
    }

    return QModelIndex{};
  }

  ProcessNode& rootNode() override
  {
    return m_root;
  }
  const ProcessNode& rootNode() const override
  {
    return m_root;
  }

  // Data reading
  int columnCount(const QModelIndex& parent) const override
  {
    return 1;
  }

  QVariant data(const QModelIndex& index, int role) const override
  {
    const auto& node = nodeFromModelIndex(index);
    switch (role)
    {
      case Qt::DisplayRole:
        return node.name;
    }
    return QVariant{};
  }

  QVariant
  headerData(int section, Qt::Orientation orientation, int role) const override
  {
    return {};
  }

  Qt::ItemFlags flags(const QModelIndex& index) const override
  {
    Qt::ItemFlags f
        = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
    return f;
  }

  // Drag, drop, etc.
  QStringList mimeTypes() const override
  {
    return {score::mime::processdata()};
  }

  QMimeData* mimeData(const QModelIndexList& indexes) const override
  {
    // Only 1 index for now
    QMimeData* mimeData = new QMimeData;

    const auto& index = indexes.first();
    const auto& node = nodeFromModelIndex(index);

    mimeData->setData(
        score::mime::processdata(), QJsonDocument{node.json}.toJson());
    return mimeData;
  }

  Qt::DropActions supportedDragActions() const override
  {
    return Qt::CopyAction;
  }
};
}
