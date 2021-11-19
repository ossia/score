#include "ProcessesItemModel.hpp"

#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/RecursiveWatch.hpp>

#include <QElapsedTimer>
#include <QIcon>
#include <QMimeData>
#include <QTimer>

#include <map>

namespace Library
{

ProcessesItemModel::ProcessesItemModel(
    const score::GUIApplicationContext& ctx,
    QObject* parent)
    : TreeNodeBasedItemModel<ProcessNode>{parent}
    , context{ctx}
{
  auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
  procs.added.connect<&ProcessesItemModel::on_newPlugin>(*this);

  auto& lib = context.settings<Library::Settings::Model>();
  con(lib, &Library::Settings::Model::rescanLibrary, this, &ProcessesItemModel::rescan);
  rescan();
}

void ProcessesItemModel::rescan()
{
  auto& procs = context.interfaces<Process::ProcessFactoryList>();

  std::map<QString, std::vector<Process::ProcessModelFactory*>> sorted;
  for (Process::ProcessModelFactory& proc : procs)
  {
    if(!(proc.descriptor({}).category & Process::ProcessCategory::Deprecated))
      sorted[proc.category()].push_back(&proc);
  }

  beginResetModel();
  m_root = ProcessNode{};
  for (auto& e : sorted)
  {
    ProcessData p;

    auto& cat = m_root.emplace_back(
        ProcessData{
            {{}, e.first, {}}, Process::getCategoryIcon(e.first), {}, {}},
        &m_root);

    for (auto p : e.second)
    {
      cat.emplace_back(
          ProcessData{
              {p->concreteKey(), p->prettyName(), {}}, QIcon{}, {}, {}},
          &cat);
    }
  }
  endResetModel();

  auto& lib = context.settings<Library::Settings::Model>();

  auto libpath = lib.getPackagesPath();

  static score::RecursiveWatch w;
  w.reset();
  w.setWatchedFolder(libpath.toStdString());
  auto& lib_setup = context.interfaces<Library::LibraryInterfaceList>();
  // TODO lib_setup.added.connect<&ProcessesItemModel::on_newPlugin>(*this);
  for (auto& lib : lib_setup)
  {
    lib.setup(*this, context);
    score::RecursiveWatch::Callbacks cbs;
    cbs.added = [&lib] (std::string_view path) { lib.addPath(path); };
    cbs.removed = [&lib] (std::string_view path) { lib.removePath(path); };
    for(const QString& ext : lib.acceptedFiles())
      w.registerWatch(ext.toStdString(), cbs);
  }

  if(!QDir{libpath}.exists())
    return;

  QTimer::singleShot(1, this, [] {
                       w.scan();
                     });
}

void ProcessesItemModel::on_newPlugin(const Process::ProcessModelFactory& fact)
{
  auto it = ossia::find_if(m_root, [&, cat = fact.category()] (ProcessData& container){
    return container.prettyName == cat;
  });
  if(it != m_root.end())
  {
    auto& cat = *it;
    cat.emplace_back(ProcessData{{fact.concreteKey(), fact.prettyName(), {}}, QIcon{}, {}, {}}, &cat);
  }
  else
  {
    auto& cat = m_root.emplace_back(
        ProcessData{
            {{}, fact.category(), {}}, Process::getCategoryIcon(fact.category()), {}, {}},
        &m_root);
    cat.emplace_back(ProcessData{{fact.concreteKey(), fact.prettyName(), {}}, QIcon{}, {}, {}}, &cat);
  }
}

QModelIndex
ProcessesItemModel::find(const Process::ProcessModelFactory::ConcreteKey& k)
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

ProcessNode& ProcessesItemModel::rootNode()
{
  return m_root;
}

const ProcessNode& ProcessesItemModel::rootNode() const
{
  return m_root;
}

int ProcessesItemModel::columnCount(const QModelIndex& parent) const
{
  return 1;
}

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

QVariant ProcessesItemModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const
{
  return {};
}

Qt::ItemFlags ProcessesItemModel::flags(const QModelIndex& index) const
{
  Qt::ItemFlags f;

  const auto& node = nodeFromModelIndex(index);
  if (node.key == Process::ProcessModelFactory::ConcreteKey{})
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

ProcessNode& addToLibrary(ProcessNode& parent, ProcessData&& data)
{
  const struct
  {
    bool operator()(const QString& lhs, const Library::ProcessData& rhs)
        const noexcept
    {
      return QString::compare(lhs, rhs.prettyName, Qt::CaseInsensitive) < 0;
    }
    bool operator()(const Library::ProcessData& lhs, const QString& rhs)
        const noexcept
    {
      return QString::compare(lhs.prettyName, rhs, Qt::CaseInsensitive) < 0;
    }
  } nameSort;
  return ossia::emplace_sorted(
      parent, data.prettyName, nameSort, std::move(data), &parent);
}

}
