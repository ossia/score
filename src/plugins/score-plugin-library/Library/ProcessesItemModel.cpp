#include "ProcessesItemModel.hpp"
#include <Library/LibraryInterface.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <score/application/GUIApplicationContext.hpp>

#include <QMimeData>
#include <QIcon>
#include <QTimer>

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
    ProcessData p;

    auto& cat = m_root.emplace_back(
          ProcessData{{{}, e.first, {}}, Process::getCategoryIcon(e.first), {}, {}}, &m_root);

    for (auto p : e.second)
    {
      cat.emplace_back(
            ProcessData{{p->concreteKey(), p->prettyName(), {}}, QIcon{}, {}, {}},
            &cat);
    }
  }

  auto& lib_setup = ctx.interfaces<Library::LibraryInterfaceList>();
  int k = 0;
  for (auto& lib : lib_setup)
  {
    QTimer::singleShot(k++ * 100, [&] {
      lib.setup(*this, ctx);
    });
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
    return node.prettyName;
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
  Qt::ItemFlags f;

  const auto& node = nodeFromModelIndex(index);
  if(node.key == Process::ProcessModelFactory::ConcreteKey{})
    f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  else
    f = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;

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
  MimeReader<Process::ProcessData>{*mimeData}.serialize(node);
  return mimeData;
}

Qt::DropActions ProcessesItemModel::supportedDragActions() const
{
  return Qt::CopyAction;
}

}
