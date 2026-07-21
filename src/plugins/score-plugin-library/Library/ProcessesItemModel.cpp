#include "ProcessesItemModel.hpp"

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/RecursiveWatch.hpp>

#include <QElapsedTimer>
#include <QIcon>
#include <QMimeData>
#include <QThreadPool>
#include <QTimer>

namespace Library
{
namespace
{
struct LibraryNameSort
{
  bool operator()(const QString& lhs, const Library::ProcessData& rhs) const noexcept
  {
    return QString::compare(lhs, rhs.prettyName, Qt::CaseInsensitive) < 0;
  }
  bool operator()(const Library::ProcessData& lhs, const QString& rhs) const noexcept
  {
    return QString::compare(lhs.prettyName, rhs, Qt::CaseInsensitive) < 0;
  }
};
}
ProcessesItemModel::ProcessesItemModel(
    const score::GUIApplicationContext& ctx, QObject* parent)
    : TreeNodeBasedItemModel<ProcessNode>{parent}
    , context{ctx}
    , m_alive{std::make_shared<std::atomic_bool>(true)}
{
  auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
  procs.added.connect<&ProcessesItemModel::on_newPlugin>(*this);

  // Node pointers handed to the search indexing worker are invalidated by
  // resets and removals; drop any in-flight result when that happens.
  auto invalidate = [this] { ++m_searchIndexGeneration; };
  connect(this, &QAbstractItemModel::modelAboutToBeReset, this, invalidate);
  connect(this, &QAbstractItemModel::rowsAboutToBeRemoved, this, invalidate);

  // Some scanners (LV2, RecursiveWatch commits...) add nodes without
  // begin/endInsertRows, so there is no reliable "node added" signal: poll
  // instead. The sweep is a cheap in-memory tree walk when there is nothing
  // new to index.
  m_searchIndexTimer.setInterval(3000);
  connect(&m_searchIndexTimer, &QTimer::timeout, this, [this] { indexForSearch(); });
  m_searchIndexTimer.start();

  auto& lib = context.settings<Library::Settings::Model>();
  con(lib, &Library::Settings::Model::rescanLibrary, this, &ProcessesItemModel::rescan);
  rescan();
}

ProcessesItemModel::~ProcessesItemModel()
{
  *m_alive = false;
}

void ProcessesItemModel::indexForSearch()
{
  if(m_searchIndexRunning)
    return;

  struct Item
  {
    ProcessNode* node{};
    Process::ProcessModelFactory* factory{};
    QString customData;
    QString prettyName;
  };

  // GUI thread: collect the nodes which still need their search data, and
  // resolve their factory here so that the worker never touches the
  // interface list (which can grow when addons are loaded).
  auto items = std::make_shared<std::vector<Item>>();
  auto& factories = context.interfaces<Process::ProcessFactoryList>();
  auto collect = [&](auto&& self, ProcessNode& node) -> void {
    if(node.key != Process::ProcessModelFactory::ConcreteKey{}
       && node.searchString.isEmpty())
    {
      if(auto* f = factories.get(node.key))
        items->push_back({&node, f, node.customData, node.prettyName});
    }
    for(auto& child : node)
      self(self, child);
  };
  collect(collect, m_root);

  if(items->empty())
    return;

  m_searchIndexRunning = true;
  const auto generation = m_searchIndexGeneration;

  // Worker thread: descriptor() is potentially very slow (it may parse the
  // file behind the node); this is the whole reason this runs off the GUI
  // thread. Results are delivered in packets to avoid clobbering the main
  // loop with tens of thousands of queued events.
  QThreadPool::globalInstance()->start(
      [this, items = std::move(items), generation, alive = m_alive] {
    constexpr std::size_t packetSize = 100;
    std::vector<std::pair<ProcessNode*, QString>> packet;
    packet.reserve(packetSize);

    auto flush = [&] {
      if(packet.empty())
        return;
      QMetaObject::invokeMethod(
          this, [this, generation, packet = std::move(packet)] {
        if(generation != m_searchIndexGeneration)
          return;
        for(const auto& [node, str] : packet)
          node->searchString = str;
      }, Qt::QueuedConnection);
      packet.clear();
      packet.reserve(packetSize);
    };

    for(const Item& item : *items)
    {
      if(!*alive)
        return;

      const auto desc = item.factory->descriptor(item.customData);
      QStringList parts{item.prettyName, desc.prettyName, desc.description};
      parts += desc.tags;
      parts.removeAll(QString{});
      parts.removeDuplicates();

      // Never leave the string empty: an empty searchString means
      // "not indexed yet" and the node would be rescanned forever.
      QString str = parts.isEmpty() ? QStringLiteral("|") : parts.join(u'|').toLower();

      packet.emplace_back(item.node, std::move(str));
      if(packet.size() >= packetSize)
        flush();
    }
    flush();

    QMetaObject::invokeMethod(
        this, [this] { m_searchIndexRunning = false; }, Qt::QueuedConnection);
  });
}

ProcessNode& ProcessesItemModel::addCategory(const QString& c)
{
  auto split = c.split("/");
  auto icon = Process::getCategoryIcon(split[0]);

  auto* node = &m_root;
  for(QString& cat : split)
  {
    // First try to find the existing node.
    bool found = false;
    for(auto& n : *node)
    {
      if(n.prettyName == cat)
      {
        node = &n;
        found = true;
        icon = {}; // Icon only the first time
        break;
      }
    }
    if(found)
      continue;

    // Otherwise add it

    auto& new_node = ossia::emplace_sorted(
        *node, cat, LibraryNameSort{}, ProcessData{{{}, cat, {}}, icon}, node);
    node = &new_node;
    icon = {}; // Icon only the first time
  }

  return *node;
}

void ProcessesItemModel::rescan()
{
  auto& procs = context.interfaces<Process::ProcessFactoryList>();

  ossia::flat_map<QString, ossia::flat_map<QString, Process::ProcessModelFactory*>>
      sorted;
  sorted.reserve(100);
  for(Process::ProcessModelFactory& proc : procs)
  {
    static_assert((1LL << 63) == (1ULL << 63));
    static_assert(sizeof(Process::ProcessFlags::Deprecated) == sizeof(1ULL));
    static_assert(sizeof(1ULL) == sizeof(uint64_t));
    if(!(proc.flags() & Process::ProcessFlags::Deprecated))
      sorted[proc.category()][proc.prettyName()] = &proc;
  }

  beginResetModel();
  m_root = ProcessNode{};
  for(auto& e : sorted)
  {
    ProcessData p;

    auto& cat = addCategory(e.first);

    for(const auto& [_, p] : e.second)
    {
      cat.emplace_back(
          ProcessData{{p->concreteKey(), p->prettyName(), {}}, QIcon{}}, &cat);
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

  for(auto& lib : lib_setup)
  {
    lib.setup(*this, context);
    for(const QString& ext : lib.acceptedFiles())
    {
      score::RecursiveWatch::AsyncCallbacks cbs;
      cbs.filter = [&lib](std::string_view path) -> std::function<void()> {
        return lib.asyncAddPath(path);
      };
      w.registerWatch(ext.toStdString(), std::move(cbs));
    }
  }

  if(!QDir{libpath}.exists())
    return;

  w.scanAsync(this);
}

void ProcessesItemModel::on_newPlugin(const score::InterfaceBase& base)
{
  auto& fact = static_cast<const Process::ProcessModelFactory&>(base);
  auto it = ossia::find_if(m_root, [&, cat = fact.category()](ProcessData& container) {
    return container.prettyName == cat;
  });
  if(it != m_root.end())
  {
    auto& cat = *it;
    const int row = cat.childCount();
    beginInsertRows(nodeToIndex(cat), row, row);
    cat.emplace_back(
        ProcessData{{fact.concreteKey(), fact.prettyName(), {}}, QIcon{}}, &cat);
    endInsertRows();
  }
  else
  {
    const int catRow = m_root.childCount();
    beginInsertRows(QModelIndex{}, catRow, catRow);
    auto& cat = m_root.emplace_back(
        ProcessData{
            {{}, fact.category(), {}}, Process::getCategoryIcon(fact.category())},
        &m_root);
    endInsertRows();

    beginInsertRows(nodeToIndex(cat), 0, 0);
    cat.emplace_back(
        ProcessData{{fact.concreteKey(), fact.prettyName(), {}}, QIcon{}}, &cat);
    endInsertRows();
  }
}

QModelIndex ProcessesItemModel::nodeToIndex(const ProcessNode& n) const
{
  auto* parent = n.parent();
  if(!parent)
    return {}; // root
  const int row = parent->indexOfChild(&n);
  if(row < 0)
    return {};
  return createIndex(row, 0, const_cast<ProcessNode*>(&n));
}

QModelIndex ProcessesItemModel::find(const Process::ProcessModelFactory::ConcreteKey& k)
{
  for(auto& cat : m_root)
  {
    auto proc_it = cat.begin();
    for(int i = 0; i < cat.childCount(); ++proc_it, ++i)
    {
      ProcessNode& proc = *proc_it;
      if(proc.key == k)
      {
        return createIndex(i, 0, &proc);
      }
      else if(proc.hasChildren())
      {
        auto subproc_it = proc.begin();
        for(int j = 0; j < proc.childCount(); ++subproc_it, ++j)
        {
          ProcessNode& subproc = *subproc_it;
          if(subproc.key == k)
          {
            return createIndex(j, 0, &subproc);
          }
        }
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
  switch(role)
  {
    case Qt::DisplayRole:
      return node.prettyName;
    case Qt::DecorationRole:
      return node.icon;
  }
  return QVariant{};
}

QVariant
ProcessesItemModel::headerData(int section, Qt::Orientation orientation, int role) const
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

ProcessNode& addToLibrary(ProcessNode& parent, ProcessData&& data)
{
  return ossia::emplace_sorted(
      parent, data.prettyName, LibraryNameSort{}, std::move(data), &parent);
}
}
