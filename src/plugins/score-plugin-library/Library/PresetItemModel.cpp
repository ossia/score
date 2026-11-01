#include "PresetItemModel.hpp"

#include <Process/ApplicationPlugin.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/File.hpp>
#include <score/tools/FilePath.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/detail/ssize.hpp>

#include <QContextMenuEvent>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>

namespace Library
{

PresetItemModel::PresetItemModel(
    const score::GUIApplicationContext& ctx, QObject* parent)
    : QAbstractItemModel{parent}
    , presets{ctx.applicationPlugin<Process::ApplicationPlugin>().presets}
    , m_plugin{&ctx.applicationPlugin<Process::ApplicationPlugin>()}
{
  // `presets` belongs to the application plugin and is fed both by the
  // library scan and by savePreset: mirror its insertions with exact ranges.
  con(*m_plugin, &Process::ApplicationPlugin::presetAboutToBeAdded, this,
      [this](int row) { beginInsertRows(QModelIndex{}, row, row); });
  con(*m_plugin, &Process::ApplicationPlugin::presetAdded, this,
      [this](int) { endInsertRows(); });
  con(*m_plugin, &Process::ApplicationPlugin::presetAboutToBeRemoved, this,
      [this](int row) { beginRemoveRows(QModelIndex{}, row, row); });
  con(*m_plugin, &Process::ApplicationPlugin::presetRemoved, this,
      [this](int) { endRemoveRows(); });
  con(*m_plugin, &Process::ApplicationPlugin::presetChanged, this, [this](int row) {
    const auto idx = index(row, 0, {});
    dataChanged(idx, idx);
  });
  con(*m_plugin, &Process::ApplicationPlugin::presetsAboutToBeCleared, this,
      [this] { beginResetModel(); });
  con(*m_plugin, &Process::ApplicationPlugin::presetsCleared, this,
      [this] { endResetModel(); });
}

QModelIndex PresetItemModel::index(int row, int column, const QModelIndex& parent) const
{
  return createIndex(row, column, nullptr);
}

QModelIndex PresetItemModel::parent(const QModelIndex& child) const
{
  return {};
}

int PresetItemModel::rowCount(const QModelIndex& parent) const
{
  return presets.size();
}

int PresetItemModel::columnCount(const QModelIndex& parent) const
{
  return 1;
}

QVariant PresetItemModel::data(const QModelIndex& index, int role) const
{
  if(index.row() < std::ssize(presets))
  {
    switch(role)
    {
      case Qt::DisplayRole:
      case Qt::EditRole:
        return presets[index.row()].name;
      case Qt::ToolTipRole:
        if(const auto& desc = presets[index.row()].description; !desc.isEmpty())
          return desc;
        break;
    }
  }

  return {};
}

PresetListView::~PresetListView() = default;
auto preset_list_view_symbol_for_shlib_bug = &PresetListView::doubleClicked;
void PresetListView::mouseDoubleClickEvent(QMouseEvent* event)
{
  auto proxy_index = indexAt(event->pos());
  if(!proxy_index.isValid())
    return;

  auto proxy = static_cast<QSortFilterProxyModel*>(this->model());
  auto model_index = proxy->mapToSource(proxy_index);

  auto& self = *safe_cast<PresetItemModel*>(proxy->sourceModel());
  if(model_index.row() < 0 || model_index.row() >= int(self.presets.size()))
    return;

  auto& preset = self.presets[model_index.row()];
  doubleClicked(preset);

  event->accept();
}

void PresetListView::paintEvent(QPaintEvent* e)
{
  QListView::paintEvent(e);

  if(model() && model()->rowCount(rootIndex()) > 0)
    return;

  QPainter p{this->viewport()};
  p.drawText(rect(), Qt::AlignLeft, "<No presets>");
}

int PresetListView::presetRow(const QModelIndex& proxy_index) const
{
  if(!proxy_index.isValid())
    return -1;
  auto proxy = static_cast<QSortFilterProxyModel*>(this->model());
  return proxy->mapToSource(proxy_index).row();
}

void PresetListView::contextMenuEvent(QContextMenuEvent* event)
{
  const int row = presetRow(indexAt(event->pos()));
  if(row < 0)
    return;
  auto proxy = static_cast<QSortFilterProxyModel*>(this->model());
  auto& self = *safe_cast<PresetItemModel*>(proxy->sourceModel());

  QMenu menu;
  auto rename = menu.addAction(tr("Rename"));
  rename->setEnabled(self.canEdit(row));
  connect(rename, &QAction::triggered, this, [this, idx = indexAt(event->pos())] {
    edit(idx);
  });
  auto remove = menu.addAction(tr("Delete"));
  remove->setEnabled(self.canEdit(row));
  connect(remove, &QAction::triggered, this, [&self, row] { self.removePreset(row); });
  if(!self.canEdit(row))
    menu.addAction(tr("Presets of installed packages cannot be changed"))
        ->setEnabled(false);
  menu.exec(event->globalPos());
  event->accept();
}

void PresetListView::keyPressEvent(QKeyEvent* event)
{
  if(event->key() == Qt::Key_Delete && state() != QAbstractItemView::EditingState)
  {
    if(const int row = presetRow(currentIndex()); row >= 0)
    {
      auto proxy = static_cast<QSortFilterProxyModel*>(this->model());
      safe_cast<PresetItemModel*>(proxy->sourceModel())->removePreset(row);
      event->accept();
      return;
    }
  }
  QListView::keyPressEvent(event);
}

static bool isValidForFilename(const QString& name)
{
  if(name.isEmpty())
    return false;

  for(QChar c : QStringLiteral(",^@=+{}[]~!?:&*\"|#%<>$\"';`'"))
    if(name.contains(c))
      return false;

  return true;
}

//! Writes the preset to `path`, made unique, under the name of the file
static bool writePresetFile(Process::Preset& preset, const QString& path)
{
  const QString unique = score::addUniqueSuffix(path);
  Process::Preset written = preset;
  written.name = QFileInfo{unique}.completeBaseName();
  written.path = unique;

  QFile f{unique};
  if(!f.open(QIODevice::WriteOnly))
    return false;
  f.write(written.toJson());
  preset = std::move(written);
  return true;
}

//! A new preset goes in the user folder, under its process (and plug-in)
static bool writeNewPreset(Process::Preset& preset)
{
  const auto& ctx = score::GUIAppContext();

  const auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
  auto& factory = *procs.get(preset.key.key);
  const auto& desc = factory.descriptor(preset.key.effect); // TODO
  QString presetFolder = ctx.settings<Library::Settings::Model>().getUserPresetsPath();

  presetFolder += "/" + factory.prettyName();

  // Put VST, etc. in subfolders
  if(desc.prettyName != factory.prettyName())
    presetFolder += "/" + desc.prettyName;

  QDir{}.mkpath(presetFolder);
  return writePresetFile(preset, presetFolder + "/" + preset.name + ".scp");
}

bool PresetItemModel::canEdit(int row) const
{
  if(!ossia::valid_index(row, presets))
    return false;
  const auto& path = presets[row].path;
  if(path.isEmpty())
    return false;
  // Packages are replaced when they are updated: only the user's own files
  const QString user = QDir::cleanPath(
      score::GUIAppContext().settings<Library::Settings::Model>().getUserLibraryPath());
  return QDir::cleanPath(QFileInfo{path}.absoluteFilePath()).startsWith(user + "/");
}

bool PresetItemModel::removePreset(int row)
{
  if(!canEdit(row))
    return false;
  // To the trash, not deleted: there is no undo in the library
  if(!QFile::moveToTrash(presets[row].path))
    return false;
  m_plugin->removePreset(row);
  return true;
}

bool PresetItemModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
  if(!index.isValid())
    return false;
  if(index.row() < 0 || index.row() >= int(presets.size()))
    return false;
  if(role != Qt::EditRole)
    return false;
  auto str = value.toString();
  if(!isValidForFilename(str))
    return false;

  const int row = index.row();
  if(!canEdit(row))
    return false;
  auto& preset = presets[row];
  if(str == preset.name)
    return true;

  // In its own folder, wherever that is in the user library
  const QString old = preset.path;
  auto renamed = preset;
  renamed.name = str;
  if(!writePresetFile(renamed, QFileInfo{old}.absolutePath() + "/" + str + ".scp"))
    return false;
  QFile::remove(old);
  preset = std::move(renamed);
  m_plugin->presetChanged(row);
  return true;
}

bool PresetItemModel::dropMimeData(
    const QMimeData* data, Qt::DropAction act, int row, int col,
    const QModelIndex& parent)
{
  const auto& ctx = score::GUIAppContext();
  const rapidjson::Document jsondoc = readJson(data->data(score::mime::layerdata()));
  if(!jsondoc.HasMember("Path"))
    return false;
  auto obj = JsonValue{jsondoc}["Path"].to<Path<Process::ProcessModel>>();
  auto doc = ctx.docManager.currentDocument();
  if(!doc)
    return false;
  const Process::ProcessModel* proc = obj.try_find(doc->context());
  if(!proc)
    return false;

  return savePreset(*proc);
}

bool PresetItemModel::savePreset(const Process::ProcessModel& proc)
{
  auto preset = proc.savePreset();
  if(!writeNewPreset(preset))
    return false;

  // Sorted insertion + exact insert signals through the connections made in
  // the constructor.
  m_plugin->addPreset(std::move(preset));
  return true;
}

bool PresetItemModel::canDropMimeData(
    const QMimeData* data, Qt::DropAction act, int row, int col,
    const QModelIndex& parent) const
{
  return data->hasFormat(score::mime::layerdata());
}

QMimeData* PresetItemModel::mimeData(const QModelIndexList& indexes) const
{
  if(indexes.empty())
    return nullptr;

  int row = indexes.front().row();
  if(row >= int(presets.size()) || row < 0)
    return nullptr;

  auto mime = new QMimeData;
  const auto& preset = presets[row];

  mime->setData(score::mime::processpreset(), preset.toJson());

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
  if(canEdit(index.row()))
    f |= Qt::ItemIsEditable;
  return f;
}

bool PresetFilterProxy::filterAcceptsRow(int srcRow, const QModelIndex& srcParent) const
{
  PresetItemModel* model = safe_cast<PresetItemModel*>(sourceModel());
  if(!ossia::valid_index(srcRow, model->presets))
    return false;
  if(model->presets[srcRow].key.key != currentFilter.key)
    return false;
  if(currentFilter.effect.isEmpty())
    return true;
  return model->presets[srcRow].key.effect == currentFilter.effect;
}

}
