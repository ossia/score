#pragma once
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Library/LibrarySettings.hpp>
#include <Library/ProcessEntry.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>
#include <score/tools/File.hpp>
#include <score/tools/RecursiveWatch.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/tools/std/StringHash.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QDir>
#include <QIcon>

#include <nano_observer.hpp>
#include <score_plugin_library_export.h>

#include <verdigris>

namespace score
{
struct GUIApplicationContext;
}

namespace Library
{
using ProcessNode = TreeNode<ProcessData>;

class SCORE_PLUGIN_LIBRARY_EXPORT ProcessesItemModel
    : public TreeNodeBasedItemModel<ProcessNode>
    , public Nano::Observer
{
public:
  ProcessesItemModel(const score::GUIApplicationContext& ctx, QObject* parent);

  void rescan();
  QModelIndex find(const Process::ProcessModelFactory::ConcreteKey& k);

  //! Queue one scanned entry for publication into the tree. The only way the
  //! tree changes outside rescan()/replaceChildren(). Entries are buffered
  //! and flushed on a 0-timer; consecutive entries sharing the same anchor
  //! and category chain are inserted as one subtree / one ranged insert, so
  //! a folder scanned in one batch costs one rowsInserted. GUI thread only.
  //!
  //! An entry with atRoot set anchors at the root instead of at a process
  //! node: what another machine offers is named by categories that exist
  //! nowhere here, so there is no key to anchor to. A key that resolves to
  //! no anchor is still dropped, as before.
  void publish(ProcessEntry&& entry);

  //! Same, from a scan whose generation was captured at rescan() time:
  //! entries from a scan that predates the current rescan are dropped.
  void publish(ProcessEntry&& entry, uint64_t generation);

  //! The scan generation the model is currently accepting. Bumped by rescan().
  uint64_t generation() const noexcept { return m_generation; }

  //! Atomically replace the children of the process node identified by key
  //! with a staged forest, given in display order (each staged node's own
  //! children are name-sorted on insertion). For the plugin databases
  //! (LV2/CLAP/VST/VST3) whose content is recomputed wholesale. Emits exact
  //! remove/insert ranges — except during rescan()'s reset, where the whole
  //! model is already inside a reset envelope. GUI thread only.
  void replaceChildren(
      const Process::ProcessModelFactory::ConcreteKey& key,
      std::vector<StagedNode> children);

  //! Marks a process node as a pure container: plugin databases clear the
  //! anchor's key so only their per-plugin children are user-creatable.
  void clearAnchorKey(const Process::ProcessModelFactory::ConcreteKey& key);

  //! Replace the whole tree with a staged forest, in display order (each
  //! staged node's own children are name-sorted on insertion), inside one
  //! reset envelope. For a library whose content is not this machine's at
  //! all: a terminal shows what the machine running the score can make, and
  //! none of its own processes are relevant. Like clear(), it drops the scan
  //! in flight and the anchors, which point into the tree being replaced.
  //! GUI thread only.
  void replaceRoot(std::vector<StagedNode> forest);

  //! Empty the tree inside a proper reset envelope: rescan()'s reset half
  //! without the repopulate. A terminal document lists no local processes,
  //! and QAbstractItemModel::begin/endResetModel are protected, so callers
  //! outside the class cannot bracket the clear themselves.
  void clear();

  //! Immediately publish everything still buffered. Mostly for tests.
  void flushPending();

  ProcessNode& rootNode() override;
  const ProcessNode& rootNode() const override;

  // Data reading
  int columnCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  // Drag, drop, etc.
  QStringList mimeTypes() const override;
  QMimeData* mimeData(const QModelIndexList& indexes) const override;
  Qt::DropActions supportedDragActions() const override;

  void on_newPlugin(const score::InterfaceBase& fact);

private:
  struct PendingEntry
  {
    ProcessEntry entry;
    uint64_t generation{};
  };

  ProcessNode& addCategory(const QString& cat);
  QModelIndex indexFromNode(ProcessNode& n);
  void publishRun(PendingEntry* entries, std::size_t count);
  void scheduleFlush();

  const score::GUIApplicationContext& context;
  ProcessNode m_root;

  // The process nodes entries anchor to, by concrete key. Owned by the tree;
  // rebuilt by rescan() and extended by on_newPlugin, so it never dangles.
  ossia::hash_map<Process::ProcessModelFactory::ConcreteKey, ProcessNode*> m_anchors;

  std::vector<PendingEntry> m_pending;
  uint64_t m_generation{};
  bool m_flushQueued{};
  bool m_inReset{};
};

}

inline QDataStream& operator<<(QDataStream& i, const Library::ProcessData& sel)
{
  return i;
}
inline QDataStream& operator>>(QDataStream& i, Library::ProcessData& sel)
{
  return i;
}

W_REGISTER_ARGTYPE(Library::ProcessData)
Q_DECLARE_METATYPE(Library::ProcessData)
W_REGISTER_ARGTYPE(std::optional<Library::ProcessData>)
