#pragma once
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/tools/std/StringHash.hpp>
#include <QIcon>
#include <QDir>

#include <score_plugin_library_export.h>

#include <unordered_map>
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
  QString author;
  QString description;
};

using ProcessNode = TreeNode<ProcessData>;
SCORE_PLUGIN_LIBRARY_EXPORT
ProcessNode& addToLibrary(ProcessNode& parent, Library::ProcessData&& data);

class SCORE_PLUGIN_LIBRARY_EXPORT ProcessesItemModel : public TreeNodeBasedItemModel<ProcessNode>
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

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  // Drag, drop, etc.
  QStringList mimeTypes() const override;
  QMimeData* mimeData(const QModelIndexList& indexes) const override;
  Qt::DropActions supportedDragActions() const override;

private:
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
  std::unordered_map<QString, Library::ProcessNode*> categories;

  void add(const QFileInfo& file, Library::ProcessData&& pdata)
  {
    auto parentFolder = file.dir().dirName();
    if(auto it = categories.find(parentFolder); it != categories.end())
    {
      Library::addToLibrary(*it->second, std::move(pdata));
    }
    else
    {
      if(file.dir() == libraryFolder)
      {
        Library::addToLibrary(*parent, std::move(pdata));
      }
      else
      {
        auto& category = Library::addToLibrary(*parent, Library::ProcessData{{{}, parentFolder, {}}, {}, {}, {}});
        Library::addToLibrary(category, std::move(pdata));
        categories[parentFolder] = &category;
      }
    }
  }
};

}

inline QDataStream& operator<<(QDataStream& i, const Library::ProcessData& sel) { return i; }
inline QDataStream& operator>>(QDataStream& i, Library::ProcessData& sel) { return i; }

W_REGISTER_ARGTYPE(Library::ProcessData)
Q_DECLARE_METATYPE(Library::ProcessData)
W_REGISTER_ARGTYPE(std::optional<Library::ProcessData>)
