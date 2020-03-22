#include "PresetItemModel.hpp"
#include <Library/LibrarySettings.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>

namespace Library
{

PresetItemModel::PresetItemModel(const score::GUIApplicationContext& ctx, QObject* parent)
  : QAbstractItemModel{parent}
{
  presets.reserve(500);
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

void PresetItemModel::registerPreset(const Process::ProcessFactoryList& procs, const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return;

  QJsonParseError ok{};
  auto preset = QJsonDocument::fromJson(f.readAll(), &ok);
  if (ok.error != QJsonParseError::NoError)
    return;

  if(auto p = Process::Preset::fromJson(procs, preset.object()))
  {
    auto it = std::lower_bound(presets.begin(), presets.end(), *p, [] (const auto& lhs, const auto& rhs) { return lhs.key < rhs.key; });

    presets.insert(it, std::move(*p));
  }
}

QModelIndex PresetItemModel::index(int row, int column, const QModelIndex& parent) const
{
  return createIndex(row, column, nullptr);
}

QModelIndex PresetItemModel::parent(const QModelIndex& child) const { return {}; }

int PresetItemModel::rowCount(const QModelIndex& parent) const
{
  return presets.size();
}

int PresetItemModel::columnCount(const QModelIndex& parent) const { return 1; }

QVariant PresetItemModel::data(const QModelIndex& index, int role) const
{
  if (index.row() < (int32_t)presets.size())
  {
    switch (role)
    {
    case Qt::DisplayRole:
      return presets[index.row()].name;
    }
  }

  return {};
}

bool PresetItemModel::dropMimeData(const QMimeData* data, Qt::DropAction act, int row, int col, const QModelIndex& parent)
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
  auto it = std::lower_bound(presets.begin(), presets.end(), preset, [] (const Process::Preset& lhs, const Process::Preset& rhs) { return lhs.key < rhs.key; });

  presets.insert(it, std::move(preset));
  endInsertRows();

  return true;
}

bool PresetItemModel::canDropMimeData(const QMimeData* data, Qt::DropAction act, int row, int col, const QModelIndex& parent) const
{
  return data->hasFormat(score::mime::layerdata());
}

QMimeData* PresetItemModel::mimeData(const QModelIndexList& indexes) const
{
  if(indexes.empty())
    return nullptr;

  int row = indexes.front().row();
  if(row >= presets.size() || row < 0)
    return nullptr;

  auto mime = new QMimeData;
  const auto& preset = presets[row];
  QByteArray data = QJsonDocument{preset.toJson()}.toJson();
  mime->setData(score::mime::processpreset(), data);

  return mime;
}

Qt::DropActions PresetItemModel::supportedDragActions() const
{
  return Qt::CopyAction;
}

Qt::DropActions PresetItemModel::supportedDropActions() const
{
  return Qt::MoveAction | Qt::CopyAction | Qt::LinkAction | Qt::IgnoreAction;
}

Qt::ItemFlags PresetItemModel::flags(const QModelIndex& index) const
{
  Qt::ItemFlags f = QAbstractItemModel::flags(index);
  f |= Qt::ItemIsDragEnabled;
  f |= Qt::ItemIsDropEnabled;
  return f;
}

bool PresetFilterProxy::filterAcceptsRow(int srcRow, const QModelIndex& srcParent) const
{
  PresetItemModel* model = safe_cast<PresetItemModel*>(sourceModel());
  return model->presets[srcRow].key.key == currentFilter;
}

}
