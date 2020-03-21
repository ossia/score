#pragma once

#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Process/Process.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProcessMimeSerialization.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/std/StringHash.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>

#include <QAbstractItemModel>
#include <QDirIterator>
#include <QIcon>
#include <QMimeData>
#include <QSortFilterProxyModel>
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

  ProcessNode& rootNode() override { return m_root; }
  const ProcessNode& rootNode() const override { return m_root; }

  // Data reading
  int columnCount(const QModelIndex& parent) const override { return 1; }

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


class PresetItemModel final : public QAbstractItemModel
{
public:
  PresetItemModel(const score::GUIApplicationContext& ctx, QObject* parent)
      : QAbstractItemModel{parent}
  {
    presets.container.reserve(500);
    auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    const QString& userLibDir = ctx.settings<Library::Settings::Model>().getPath();
    QDirIterator it(
          userLibDir,
          QStringList{"*.scorepreset"},
          QDir::Files,
          QDirIterator::Subdirectories);

    while (it.hasNext())
    {
      registerPreset(procs, it.next());
    }
  }

  void registerPreset(const Process::ProcessFactoryList& procs, const QString& path)
  {
    QFile f{path};
    if(!f.open(QIODevice::ReadOnly))
      return;

    QJsonParseError ok{};
    auto preset = QJsonDocument::fromJson(f.readAll(), &ok);
    if (ok.error != QJsonParseError::NoError)
      return;

    if(auto p = Process::Preset::fromJson(procs, preset.object()))
      presets.insert(std::move(*p));
  }


  QModelIndex
  index(int row, int column, const QModelIndex& parent) const override
  {
    return createIndex(row, column, nullptr);
  }

  QModelIndex parent(const QModelIndex& child) const override { return {}; }
  int rowCount(const QModelIndex& parent) const override
  {
    return presets.size();
  }

  int columnCount(const QModelIndex& parent) const override { return 1; }

  QVariant data(const QModelIndex& index, int role) const override
  {
    if (index.row() < (int32_t)presets.size())
    {
      switch (role)
      {
        case Qt::DisplayRole:
          return presets.container[index.row()].name;
      }
    }

    return {};
  }

  bool dropMimeData(const QMimeData* data, Qt::DropAction act, int row, int col, const QModelIndex& parent) override
  {
    const auto& ctx = score::GUIAppContext();
    auto json = data->data(score::mime::layerdata());
    auto obj = fromJsonObject<Path<Process::ProcessModel>>(QJsonDocument::fromJson(json).object()["Path"]);
    auto doc = ctx.docManager.currentDocument();
    if(!doc)
      return false;
    const Process::ProcessModel* proc = obj.try_find(doc->context());
    if(!proc)
      return false;

    auto preset = proc->savePreset();
    QJsonObject presetObj = preset.toJson();

    const auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
    const auto& desc = (*procs.find(proc->concreteKey())).descriptor(""); // TODO
    const QString& userLibDir = ctx.settings<Library::Settings::Model>().getPath();
    const QString& presetFolder = userLibDir + "/Presets/" + desc.prettyName;
    QDir{}.mkpath(presetFolder);
    QString presetPath = presetFolder + "/" + preset.name + ".scorepreset";
    if(QFile::exists(presetPath))
    {
      presetPath = presetFolder + "/" + preset.name + " (%1).scorepreset";

      int idx = 1;
      while(QFile::exists(presetPath.arg(idx)))
        idx++;
      presetPath = presetPath.arg(idx);
      preset.name = preset.name + QString("(%1)").arg(idx);
      presetObj["Name"] = preset.name;
    }

    QFile f{presetPath};
    if(!f.open(QIODevice::WriteOnly))
      return false;
    f.write(QJsonDocument{presetObj}.toJson().data());

    beginInsertRows(QModelIndex(), presets.size(), presets.size());
    presets.insert(preset);
    endInsertRows();

    return true;
  }


  bool canDropMimeData(const QMimeData* data, Qt::DropAction act, int row, int col, const QModelIndex& parent) const override
  {
    return data->hasFormat(score::mime::layerdata());
  }

  QMimeData* mimeData(const QModelIndexList &indexes) const override
  {
    if(indexes.empty())
      return nullptr;

    int row = indexes.front().row();
    if(row >= presets.size() || row < 0)
      return nullptr;

    auto mime = new QMimeData;
    const auto& preset = presets.container[row];
    QByteArray data = QJsonDocument{preset.toJson()}.toJson();
    mime->setData(score::mime::processpreset(), data);

    return mime;
  }

  Qt::DropActions supportedDragActions() const override
  {
    return Qt::CopyAction;
  }

  Qt::DropActions supportedDropActions() const override
  {
    return Qt::MoveAction | Qt::CopyAction | Qt::LinkAction | Qt::IgnoreAction;
  }

  Qt::ItemFlags flags(const QModelIndex& index) const override
  {
    Qt::ItemFlags f = QAbstractItemModel::flags(index);
    f |= Qt::ItemIsDragEnabled;
    f |= Qt::ItemIsDropEnabled;
    return f;
  }
  ossia::flat_set<Process::Preset> presets;
};

class PresetFilterProxy final : public QSortFilterProxyModel
{
public:
  using QSortFilterProxyModel::QSortFilterProxyModel;
  using QSortFilterProxyModel::invalidate;

  UuidKey<Process::ProcessModel> currentFilter{};
private:
  bool filterAcceptsRow(int srcRow, const QModelIndex& srcParent) const override
  {
    PresetItemModel* model = safe_cast<PresetItemModel*>(sourceModel());
    return model->presets.container[srcRow].key.key == currentFilter;
  }
};
}
