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
#include <QPointer>
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
  bool operator()(
      const Library::ProcessData& lhs, const Library::ProcessData& rhs) const noexcept
  {
    return QString::compare(lhs.prettyName, rhs.prettyName, Qt::CaseInsensitive) < 0;
  }
};

ProcessNode* findChildByName(ProcessNode& parent, const QString& name) noexcept
{
  for(auto& child : parent)
    if(QString::compare(child.prettyName, name, Qt::CaseInsensitive) == 0)
      return &child;
  return nullptr;
}

// Recursively turn staged data into a detached node subtree, children sorted.
ProcessNode toNode(StagedNode&& staged) noexcept
{
  ProcessNode n{std::move(staged.data), nullptr};
  for(auto& c : staged.children)
  {
    auto it = std::lower_bound(n.begin(), n.end(), c.data.prettyName, LibraryNameSort{});
    n.emplace(it, toNode(std::move(c)));
  }
  return n;
}
}

ProcessesItemModel::ProcessesItemModel(
    const score::GUIApplicationContext& ctx, QObject* parent)
    : TreeNodeBasedItemModel<ProcessNode>{parent}
    , context{ctx}
{
  auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
  procs.added.connect<&ProcessesItemModel::on_newPlugin>(*this);

  auto& lib = context.settings<Library::Settings::Model>();
  con(lib, &Library::Settings::Model::rescanLibrary, this, &ProcessesItemModel::rescan);
  rescan();
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

void ProcessesItemModel::clear()
{
  m_generation++;
  m_pending.clear();
  m_anchors.clear();

  beginResetModel();
  m_inReset = true;
  m_root = ProcessNode{};
  m_inReset = false;
  endResetModel();
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

  // Entries queued by a scan that predates this rescan target nodes that are
  // about to be destroyed: drop them.
  m_generation++;
  m_pending.clear();
  m_anchors.clear();

  auto& lib_setup = context.interfaces<Library::LibraryInterfaceList>();

  beginResetModel();
  m_inReset = true;
  m_root = ProcessNode{};
  for(auto& e : sorted)
  {
    auto& cat = addCategory(e.first);

    for(const auto& [_, p] : e.second)
    {
      auto& node = cat.emplace_back(
          ProcessData{{p->concreteKey(), p->prettyName(), {}}, QIcon{}}, &cat);
      m_anchors[p->concreteKey()] = &node;
    }
  }

  // Handler setup runs while the model is still resetting: bulk populators
  // (plugin databases) insert whole subtrees here, and doing it inside the
  // reset envelope is what makes those insertions legal without signals.
  for(auto& lib : lib_setup)
    lib.setup(*this, context);

  m_inReset = false;
  endResetModel();

  auto& libsettings = context.settings<Library::Settings::Model>();

  auto libpath = libsettings.getPackagesPath();

  static score::RecursiveWatch w;
  w.reset();
  w.setWatchedFolder(libpath.toStdString());

  for(auto& lib : lib_setup)
  {
    for(const QString& ext : lib.acceptedFiles())
    {
      score::RecursiveWatch::AsyncCallbacks cbs;
      cbs.filter = [&lib, model = QPointer{this},
                    gen = m_generation](std::string_view path) -> std::function<void()> {
        // Worker thread: pure scan. The commit closure only carries data and
        // publishes it on the GUI thread; a rescan in between just drops it.
        if(auto entry = lib.scanPath(path))
        {
          return [model, gen, e = std::move(*entry)]() mutable {
            if(model)
              model->publish(std::move(e), gen);
          };
        }
        // Handlers that commit outside this model (e.g. the QML module panel,
        // the preset library).
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
  if(fact.flags() & Process::ProcessFlags::Deprecated)
    return;

  auto it = ossia::find_if(m_root, [&, cat = fact.category()](ProcessData& container) {
    return container.prettyName == cat;
  });
  ProcessNode* cat{};
  if(it != m_root.end())
  {
    cat = &*it;
  }
  else
  {
    const QString name = fact.category();
    auto pos = std::lower_bound(m_root.begin(), m_root.end(), name, LibraryNameSort{});
    const int row = std::distance(m_root.begin(), pos);
    beginInsertRows(QModelIndex{}, row, row);
    cat = &m_root.emplace(
        pos, ProcessData{{{}, name, {}}, Process::getCategoryIcon(name)}, &m_root);
    endInsertRows();
  }

  const QString name = fact.prettyName();
  auto pos = std::lower_bound(cat->begin(), cat->end(), name, LibraryNameSort{});
  const int row = std::distance(cat->begin(), pos);
  beginInsertRows(indexFromNode(*cat), row, row);
  auto& node
      = cat->emplace(pos, ProcessData{{fact.concreteKey(), name, {}}, QIcon{}}, cat);
  endInsertRows();
  m_anchors[fact.concreteKey()] = &node;
}

QModelIndex ProcessesItemModel::find(const Process::ProcessModelFactory::ConcreteKey& k)
{
  if(auto it = m_anchors.find(k); it != m_anchors.end())
    return indexFromNode(*it->second);
  return QModelIndex{};
}

QModelIndex ProcessesItemModel::indexFromNode(ProcessNode& n)
{
  if(&n == &m_root)
    return QModelIndex{};
  auto parent = n.parent();
  SCORE_ASSERT(parent);
  const int row = parent->indexOfChild(&n);
  SCORE_ASSERT(row != -1);
  return createIndex(row, 0, &n);
}

void ProcessesItemModel::publish(ProcessEntry&& entry)
{
  publish(std::move(entry), m_generation);
}

void ProcessesItemModel::publish(ProcessEntry&& entry, uint64_t generation)
{
  if(generation != m_generation)
    return;
  m_pending.push_back(PendingEntry{std::move(entry), generation});
  scheduleFlush();
}

void ProcessesItemModel::scheduleFlush()
{
  if(m_flushQueued)
    return;
  m_flushQueued = true;
  // 0-timer: all the publishes of one RecursiveWatch batch (they arrive in a
  // single event-loop turn) coalesce into one flush.
  QTimer::singleShot(0, this, [this] { flushPending(); });
}

void ProcessesItemModel::flushPending()
{
  m_flushQueued = false;
  if(m_pending.empty())
    return;

  auto pending = std::move(m_pending);
  m_pending.clear();

  // Group runs of consecutive entries sharing anchor + category chain: the
  // filesystem scan delivers directory contents consecutively, so this makes
  // one insert per directory, not one per file.
  std::size_t i = 0;
  while(i < pending.size())
  {
    if(pending[i].generation != m_generation)
    {
      i++;
      continue;
    }
    std::size_t j = i + 1;
    while(
        j < pending.size() && pending[j].generation == m_generation
        && pending[j].entry.rootKey == pending[i].entry.rootKey
        && pending[j].entry.categoryPath == pending[i].entry.categoryPath)
      j++;

    publishRun(&pending[i], j - i);
    i = j;
  }
}

void ProcessesItemModel::publishRun(PendingEntry* entries, std::size_t count)
{
  SCORE_ASSERT(count > 0);
  auto anchor_it = m_anchors.find(entries[0].entry.rootKey);
  if(anchor_it == m_anchors.end())
    return; // Not a registered process: nothing to attach to.

  // Walk the existing part of the category chain.
  ProcessNode* parent = anchor_it->second;
  const QStringList& path = entries[0].entry.categoryPath;
  int existing = 0;
  for(; existing < path.size(); existing++)
  {
    auto child = findChildByName(*parent, path[existing]);
    if(!child)
      break;
    parent = child;
  }

  if(existing < path.size())
  {
    // The rest of the chain does not exist: build it detached, hang every
    // entry under the leaf, splice the whole thing with a single insert.
    ProcessNode staged{ProcessData{{{}, path[existing], {}}, {}}, nullptr};
    ProcessNode* leaf = &staged;
    for(int c = existing + 1; c < path.size(); c++)
      leaf = &leaf->emplace_back(ProcessData{{{}, path[c], {}}, {}}, leaf);

    for(std::size_t e = 0; e < count; e++)
    {
      auto node = toNode(std::move(entries[e].entry.node));
      auto it = std::lower_bound(
          leaf->begin(), leaf->end(), node.prettyName, LibraryNameSort{});
      leaf->emplace(it, std::move(node));
    }

    auto pos = std::lower_bound(
        parent->begin(), parent->end(), staged.prettyName, LibraryNameSort{});
    const int row = std::distance(parent->begin(), pos);
    beginInsertRows(indexFromNode(*parent), row, row);
    parent->emplace(pos, std::move(staged));
    endInsertRows();
    return;
  }

  // The full chain exists: insert the entries as contiguous sorted sub-runs.
  std::vector<ProcessNode> nodes;
  nodes.reserve(count);
  for(std::size_t e = 0; e < count; e++)
    nodes.push_back(toNode(std::move(entries[e].entry.node)));
  std::sort(nodes.begin(), nodes.end(), LibraryNameSort{});

  const QModelIndex parentIndex = indexFromNode(*parent);
  auto it = parent->begin();
  int row = 0;
  std::size_t n = 0;
  while(n < nodes.size())
  {
    // Advance to the sorted insertion point of nodes[n].
    while(it != parent->end() && LibraryNameSort{}(*it, nodes[n]))
    {
      ++it;
      ++row;
    }
    // Everything that sorts before (or equal to) the sibling at `it` forms
    // one contiguous range.
    std::size_t last = n;
    while(last < nodes.size()
          && (it == parent->end() || !LibraryNameSort{}(*it, nodes[last])))
      last++;

    beginInsertRows(parentIndex, row, row + int(last - n) - 1);
    for(; n < last; n++)
    {
      parent->insert(it, std::move(nodes[n]));
      ++row;
    }
    endInsertRows();
  }
}

void ProcessesItemModel::replaceChildren(
    const Process::ProcessModelFactory::ConcreteKey& key,
    std::vector<StagedNode> children)
{
  auto anchor_it = m_anchors.find(key);
  if(anchor_it == m_anchors.end())
    return;
  ProcessNode& parent = *anchor_it->second;

  if(m_inReset)
  {
    // rescan() already wraps the whole model in a reset: mutate directly.
    parent.resize(0);
    for(auto& c : children)
      parent.push_back(toNode(std::move(c)));
    return;
  }

  const QModelIndex parentIndex = indexFromNode(parent);
  if(const int n = parent.childCount(); n > 0)
  {
    beginRemoveRows(parentIndex, 0, n - 1);
    parent.resize(0);
    endRemoveRows();
  }

  if(!children.empty())
  {
    beginInsertRows(parentIndex, 0, int(children.size()) - 1);
    for(auto& c : children)
      parent.push_back(toNode(std::move(c)));
    endInsertRows();
  }
}

void ProcessesItemModel::clearAnchorKey(
    const Process::ProcessModelFactory::ConcreteKey& key)
{
  auto anchor_it = m_anchors.find(key);
  if(anchor_it == m_anchors.end())
    return;
  anchor_it->second->key = {};
  if(!m_inReset)
  {
    const auto idx = indexFromNode(*anchor_it->second);
    dataChanged(idx, idx);
  }
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
  if(!index.isValid())
    return Qt::NoItemFlags;

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
