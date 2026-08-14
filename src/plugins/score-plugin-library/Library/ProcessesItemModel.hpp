#pragma once
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Library/LibrarySettings.hpp>

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
#include <QTimer>

#include <atomic>
#include <memory>

#include <nano_observer.hpp>
#include <score_plugin_library_export.h>

#include <verdigris>

namespace score
{
struct GUIApplicationContext;
}

namespace Library
{
struct ProcessData : Process::ProcessData
{
  QIcon icon;

  //! Lower-case, '|'-separated search data (name, description, tags...).
  //! Empty until the async deep scan has indexed this node; the search
  //! filter then matches against it in-memory, without ever calling
  //! ProcessModelFactory::descriptor() on the search path.
  QString searchString;
};

using ProcessNode = TreeNode<ProcessData>;
SCORE_PLUGIN_LIBRARY_EXPORT
ProcessNode& addToLibrary(ProcessNode& parent, Library::ProcessData&& data);

class SCORE_PLUGIN_LIBRARY_EXPORT ProcessesItemModel
    : public TreeNodeBasedItemModel<ProcessNode>
    , public Nano::Observer
{
public:
  using QAbstractItemModel::beginInsertRows;
  using QAbstractItemModel::endInsertRows;

  using QAbstractItemModel::beginRemoveRows;
  using QAbstractItemModel::endRemoveRows;

  // async scanners (LV2, CLAP) rebuild whole subtrees on descriptorsChanged
  using QAbstractItemModel::beginResetModel;
  using QAbstractItemModel::endResetModel;

  ProcessesItemModel(const score::GUIApplicationContext& ctx, QObject* parent);
  ~ProcessesItemModel();

  void rescan();
  QModelIndex find(const Process::ProcessModelFactory::ConcreteKey& k);

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
  ProcessNode& addCategory(const QString& cat);

  //! QModelIndex pointing at \a n (invalid for the root), so that scanners can
  //! wrap their tree mutations in begin/endInsertRows instead of resetting.
  QModelIndex nodeToIndex(const ProcessNode& n) const;

  // Second-level async scan: computes ProcessData::searchString (tags,
  // description...) on a worker thread, off the fast name-only scan and off
  // the search path. Results come back to the GUI thread in packets.
  void indexForSearch();

  const score::GUIApplicationContext& context;
  ProcessNode m_root;

  QTimer m_searchIndexTimer;
  //! Bumped whenever ProcessNode pointers may dangle (model reset, row
  //! removal); in-flight indexing results from older generations are dropped.
  uint64_t m_searchIndexGeneration{};
  bool m_searchIndexRunning{};
  std::shared_ptr<std::atomic_bool> m_alive;
};

/** Utility class to organize a library in subcategories that depend
 *  on a file organization on disk, e.g. if it looks for .dsp files, it will create a subcategory
 *  for each folder which contains .dsp files.
 */
struct Subcategories
{
  Library::ProcessNode* parent{};
  QDir libraryFolder;
  std::string libraryFolderPath{};
  std::string defaultPresetsPath{};
  ossia::hash_map<QString, Library::ProcessNode*> categories;

  void init(
      std::string process_name, const QModelIndex& idx,
      const score::ApplicationContext& ctx)
  {
    categories.clear();
    parent = reinterpret_cast<Library::ProcessNode*>(idx.internalPointer());
    SCORE_ASSERT(parent);

    // We use the parent folder as category...
    libraryFolder.setPath(ctx.settings<Library::Settings::Model>().getPackagesPath());
    libraryFolderPath = libraryFolder.absolutePath().toStdString();

    // Also so that stuff in Presets/<Name of the process> does not needlessly go into a subfolder
    defaultPresetsPath = "Presets/" + process_name;
  }

  [[deprecated]] void add(const QFileInfo& file, Library::ProcessData&& pdata)
  {
    SCORE_ASSERT(parent);
    auto parentFolder = file.dir().dirName();
    if(auto it = categories.find(parentFolder); it != categories.end())
    {
      Library::addToLibrary(*it->second, std::move(pdata));
    }
    else
    {
      if(file.dir() == libraryFolder
         || file.absolutePath().endsWith(defaultPresetsPath.c_str()))
      {
        Library::addToLibrary(*parent, std::move(pdata));
      }
      else
      {
        auto& category = Library::addToLibrary(
            *parent, Library::ProcessData{{{}, parentFolder, {}}, {}});
        Library::addToLibrary(category, std::move(pdata));
        categories[parentFolder] = &category;
      }
    }
  }

  void add(const score::PathInfo& file, Library::ProcessData&& pdata)
  {
    SCORE_ASSERT(parent);
    auto parentFolder
        = QString::fromUtf8(file.parentDirName.data(), file.parentDirName.size());
    if(auto it = categories.find(parentFolder); it != categories.end())
    {
      Library::addToLibrary(*it->second, std::move(pdata));
    }
    else
    {
      if(file.absolutePath == libraryFolderPath
         || file.absolutePath.ends_with(defaultPresetsPath))
      {
        Library::addToLibrary(*parent, std::move(pdata));
      }
      else
      {
        auto& category = Library::addToLibrary(
            *parent, Library::ProcessData{{{}, parentFolder, {}}, {}});
        Library::addToLibrary(category, std::move(pdata));
        categories[parentFolder] = &category;
      }
    }
  }
};

/// Typed convenience wrapper: builds a RecursiveWatch::AsyncCallbacks
/// from a typed filter/commit pair so that consumers don't have to
/// manually wrap closures.
///
/// \a filter is called on the worker thread; return std::nullopt to reject.
/// \a commit is called on the GUI thread with the path and the accepted payload.
template <typename T>
score::RecursiveWatch::AsyncCallbacks makeAsyncCallbacks(
    std::function<std::optional<T>(std::string_view)> filter,
    std::function<void(std::string_view, T&&)> commit)
{
  struct State
  {
    std::function<std::optional<T>(std::string_view)> filter;
    std::function<void(std::string_view, T&&)> commit;
  };
  auto state = std::make_shared<State>(State{std::move(filter), std::move(commit)});

  return {.filter = [state = std::move(state)](std::string_view path)
              -> std::function<void()> {
    auto result = state->filter(path);
    if(!result)
      return {};

    return [commit = state->commit, p = std::string(path),
            data = std::move(*result)]() mutable { commit(p, std::move(data)); };
  }};
}

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
