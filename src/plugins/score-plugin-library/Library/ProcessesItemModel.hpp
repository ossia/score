#pragma once
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>
#include <score/tools/File.hpp>
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
struct ProcessData : Process::ProcessData
{
  QIcon icon;
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

  ProcessesItemModel(const score::GUIApplicationContext& ctx, QObject* parent);

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

  void on_newPlugin(const Process::ProcessModelFactory& fact);

private:
  ProcessNode& addCategory(const QString& cat);
  const score::GUIApplicationContext& context;
  ProcessNode m_root;
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
