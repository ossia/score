#include "ProcessesItemModel.hpp"
#include <Library/LibraryInterface.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <score/application/GUIApplicationContext.hpp>

#include <QMimeData>
#include <QIcon>
#include <QJsonDocument>

#include <map>

namespace Library
{

ProcessesItemModel::ProcessesItemModel(const score::GUIApplicationContext& ctx, QObject* parent)
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
          ProcessData{e.first, Process::getCategoryIcon(e.first), QJsonObject{}, {}}, &m_root);

    for (auto p : e.second)
    {
      QJsonObject obj;
      obj["Type"] = "Process";
      if(!(p->flags() & Process::ProcessFlags::RequiresCustomData))
        obj["Data"] = p->customConstructionData();
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

QModelIndex ProcessesItemModel::find(const Process::ProcessModelFactory::ConcreteKey& k)
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

ProcessNode& ProcessesItemModel::rootNode() { return m_root; }

const ProcessNode& ProcessesItemModel::rootNode() const { return m_root; }

int ProcessesItemModel::columnCount(const QModelIndex& parent) const { return 1; }

QVariant ProcessesItemModel::data(const QModelIndex& index, int role) const
{
  const auto& node = nodeFromModelIndex(index);
  switch (role)
  {
  case Qt::DisplayRole:
    return node.name;
  case Qt::DecorationRole:
      return node.icon;
  }
  return QVariant{};
}

QVariant ProcessesItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  return {};
}

Qt::ItemFlags ProcessesItemModel::flags(const QModelIndex& index) const
{
  Qt::ItemFlags f
      = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
  return f;
}

QStringList ProcessesItemModel::mimeTypes() const
{
  return {score::mime::processdata()};
}

QMimeData* ProcessesItemModel::mimeData(const QModelIndexList& indexes) const
{
  // Only 1 index for now
  QMimeData* mimeData = new QMimeData;

  const auto& index = indexes.first();
  const auto& node = nodeFromModelIndex(index);

  mimeData->setData(
        score::mime::processdata(), QJsonDocument{node.json}.toJson());
  return mimeData;
}

Qt::DropActions ProcessesItemModel::supportedDragActions() const
{
  return Qt::CopyAction;
}

}
