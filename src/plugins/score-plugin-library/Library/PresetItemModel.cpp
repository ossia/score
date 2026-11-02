#include "PresetItemModel.hpp"

#include <Process/ApplicationPlugin.hpp>
#include <Process/PresetTree.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Library/LibrarySettings.hpp>
#include <Library/PresetFile.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/File.hpp>
#include <score/tools/FilePath.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/ssize.hpp>

#include <QDir>
#include <QFont>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QTimer>

#include <utility>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Library::PresetItemModel)

namespace Library
{
//! Which file a preset dragged within the library comes from
static constexpr auto presetFileMime = "application/x-score-preset-file";

struct PresetItemModel::Node
{
  QString name;
  //! Its row in `presets`; -1 for a folder
  int preset = -1;
  //! Or its index in the target's own presets
  int builtin = -1;
  //! A folder of the target's own presets
  bool builtins = false;
  Node* parent{};
  int row{};
  std::vector<std::unique_ptr<Node>> children;
};

PresetItemModel::PresetItemModel(
    const score::GUIApplicationContext& ctx, QObject* parent)
    : QAbstractItemModel{parent}
    , presets{ctx.applicationPlugin<Process::ApplicationPlugin>().presets}
    , m_plugin{&ctx.applicationPlugin<Process::ApplicationPlugin>()}
    , m_root{std::make_unique<Node>()}
{
  // `presets` is sorted by kind of process and fed by the library's scans:
  // most changes are to presets of other processes, whose rows only shift
  const auto shift = [this](int from, int by) {
    const auto visit = [&](auto& self, Node& n) -> void {
      if(n.preset >= from)
        n.preset += by;
      for(auto& c : n.children)
        self(self, *c);
    };
    visit(visit, *m_root);
  };
  con(*m_plugin, &Process::ApplicationPlugin::presetAdded, this,
      [this, shift](int row) {
    shift(row, 1);
    if(shows(presets[row]))
      rebuildSoon();
  });
  con(*m_plugin, &Process::ApplicationPlugin::presetAboutToBeRemoved, this,
      [this](int row) {
    m_removingShown = shows(presets[row]);
    if(m_removingShown)
      beginResetModel();
  });
  con(*m_plugin, &Process::ApplicationPlugin::presetRemoved, this,
      [this, shift](int row) {
    if(m_removingShown)
    {
      build();
      endResetModel();
    }
    else
    {
      shift(row + 1, -1);
    }
    m_removingShown = false;
  });
  con(*m_plugin, &Process::ApplicationPlugin::presetChanged, this, [this](int row) {
    if(shows(presets[row]))
      rebuildSoon();
  });
  con(*m_plugin, &Process::ApplicationPlugin::presetsAboutToBeCleared, this,
      [this] { beginResetModel(); });
  con(*m_plugin, &Process::ApplicationPlugin::presetsCleared, this, [this] {
    build();
    endResetModel();
  });
}

PresetItemModel::~PresetItemModel() = default;

void PresetItemModel::setProcess(std::optional<Process::ProcessIdentifier> id)
{
  if(id == m_process)
    return;
  m_process = std::move(id);
  rebuild();
}

void PresetItemModel::setFilterText(const QString& text)
{
  if(text == m_filter)
    return;
  m_filter = text;
  rebuild();
}

void PresetItemModel::setTarget(const Process::ProcessModel* proc)
{
  if(proc == m_target)
    return;
  if(m_target)
    disconnect(m_target, nullptr, this, nullptr);
  m_target = proc;
  m_builtinsOf = nullptr;
  if(proc)
  {
    // Another script, or a plug-in reloaded: its own presets may be others
    const auto stale = [this] {
      m_builtinsOf = nullptr;
      rebuildSoon();
    };
    connect(proc, &Process::ProcessModel::inletsChanged, this, stale);
    connect(proc, &Process::ProcessModel::outletsChanged, this, stale);
  }
  if(proc)
    connect(proc, &QObject::destroyed, this, [this] {
      // Emitted by ~QObject: the process is no longer one to ask anything
      m_target = nullptr;
      rebuild();
    });
  rebuild();
}

void PresetItemModel::refreshCurrent()
{
  const auto visit = [&](auto& self, const Node& n) -> void {
    for(auto& c : n.children)
    {
      if(c->preset >= 0 || c->builtin >= 0)
      {
        const auto idx = indexOf(c.get());
        dataChanged(idx, idx, {Qt::FontRole});
      }
      self(self, *c);
    }
  };
  visit(visit, *m_root);
}

QModelIndex PresetItemModel::currentPresetIndex() const
{
  if(!m_target)
    return {};
  const auto find = [&](auto& self, const Node& n) -> const Node* {
    for(auto& c : n.children)
    {
      if(auto p = preset(indexOf(c.get())); p && Process::isCurrentPreset(*m_target, *p))
        return c.get();
      if(auto found = self(self, *c))
        return found;
    }
    return nullptr;
  };
  return indexOf(find(find, *m_root));
}

bool PresetItemModel::shows(const Process::Preset& p) const noexcept
{
  if(m_target)
    return m_target->presetMatches(p);
  return m_process && p.key.key == m_process->key
         && (m_process->effect.isEmpty() || p.key.effect == m_process->effect);
}

void PresetItemModel::rebuild()
{
  beginResetModel();
  build();
  endResetModel();
}

void PresetItemModel::rebuildSoon()
{
  if(m_coalescing)
  {
    m_stale = true;
    return;
  }
  rebuild();
  m_coalescing = true;
  QTimer::singleShot(0, this, [this] {
    m_coalescing = false;
    if(std::exchange(m_stale, false))
      rebuild();
  });
}

void PresetItemModel::build()
{
  m_root = std::make_unique<Node>();
  if(!m_process && !m_target)
  {
    m_builtins.clear();
    m_builtinsOf = nullptr;
    return;
  }

  // A plug-in's own presets, read from it once: asking can be slow (a
  // plug-in's programs one by one, thousands of preset files)
  if(!m_target)
    m_builtins.clear();
  else if(m_builtinsOf != m_target)
  {
    m_builtins = m_target->builtinPresets();
    m_builtinsOf = m_target;
  }
  const auto tree = m_target
                        ? Process::PresetTree{*m_target, presets, m_builtins}
                        : Process::PresetTree{*m_process, presets};
  const auto matches = [this](const Process::Preset& p) {
    return m_filter.isEmpty() || p.name.contains(m_filter, Qt::CaseInsensitive)
           || p.category.contains(m_filter, Qt::CaseInsensitive);
  };
  // A folder none of whose presets match is left out
  const auto copy = [&](auto& self, const Process::PresetTree::Node& from, Node& to) -> void {
    for(auto& e : from.entries)
    {
      auto n = std::make_unique<Node>();
      n->parent = &to;
      if(e.preset)
      {
        if(!matches(*e.preset))
          continue;
        n->name = e.preset->name;
        if(e.preset >= presets.data() && e.preset < presets.data() + presets.size())
          n->preset = int(e.preset - presets.data());
        else
          n->builtin = int(e.preset - m_builtins.data());
      }
      else
      {
        const auto& child = *from.children[std::size_t(e.child)];
        n->name = child.name;
        self(self, child, *n);
        if(n->children.empty())
          continue;
        n->builtins = std::any_of(n->children.begin(), n->children.end(), [](auto& c) {
          return c->builtin >= 0 || c->builtins;
        });
      }
      n->row = int(to.children.size());
      to.children.push_back(std::move(n));
    }
  };
  copy(copy, tree.root, *m_root);

  if(m_filter.isEmpty())
    for(const auto& [key, path] : m_emptyCategories)
      if(key == categoryKey())
        folder(path);
}

QString PresetItemModel::categoryKey() const
{
  if(m_target)
    return QString::fromLatin1(score::uuids::toByteArray(m_target->concreteKey().impl()))
           + m_target->effect();
  if(m_process)
    return QString::fromLatin1(score::uuids::toByteArray(m_process->key.impl()))
           + m_process->effect;
  return {};
}

PresetItemModel::Node& PresetItemModel::folder(const QStringList& path)
{
  // Among the folders, by name, and before the target's own presets
  Node* current = m_root.get();
  for(const auto& part : path)
  {
    auto& cs = current->children;
    auto it = std::find_if(cs.begin(), cs.end(), [&](auto& c) {
      return c->preset < 0 && c->builtin < 0 && c->name == part && !c->builtins;
    });
    if(it == cs.end())
    {
      auto pos = std::find_if(cs.begin(), cs.end(), [&](auto& c) {
        return (c->preset < 0 && c->builtin < 0 && part < c->name) || c->builtins;
      });
      auto n = std::make_unique<Node>();
      n->name = part;
      n->parent = current;
      it = cs.insert(pos, std::move(n));
      for(int i = 0; i < int(cs.size()); i++)
        cs[std::size_t(i)]->row = i;
    }
    current = it->get();
  }
  return *current;
}

bool PresetItemModel::isBuiltin(const QModelIndex& index) const
{
  const Node* n = index.isValid() ? node(index) : nullptr;
  if(!n)
    return false;
  if(n->builtin >= 0)
    return true;
  // Folders hold one kind of preset: the top-level one says which
  while(n->parent && n->parent != m_root.get())
    n = n->parent;
  return n->builtins || n->builtin >= 0;
}

QStringList PresetItemModel::categories() const
{
  QStringList res;
  const auto visit = [&](auto& self, const Node& n, const QString& prefix) -> void {
    for(auto& c : n.children)
    {
      if(c->preset >= 0 || c->builtin >= 0 || c->builtins)
        continue;
      const QString path = prefix.isEmpty() ? c->name : prefix + "/" + c->name;
      res.push_back(path);
      self(self, *c, path);
    }
  };
  visit(visit, *m_root, {});
  return res;
}

QModelIndex PresetItemModel::addCategory(const QStringList& path)
{
  if(path.isEmpty() || categoryKey().isEmpty())
    return {};
  for(const auto& part : path)
    if(part.trimmed().isEmpty() || part.contains('/'))
      return {};
  m_emptyCategories.emplace_back(categoryKey(), path);
  rebuild();
  return indexOfFolder(path);
}

bool PresetItemModel::movePreset(int row, const QString& category)
{
  if(!canEdit(row))
    return false;
  auto& preset = presets[row];
  if(preset.category == category)
    return true;

  auto moved = preset;
  moved.category = category;
  if(!writePresetData(preset.path, moved.toJson()))
    return false;
  preset = std::move(moved);
  m_plugin->presetChanged(row);
  return true;
}

PresetItemModel::Node* PresetItemModel::node(const QModelIndex& index) const
{
  return index.isValid() ? static_cast<Node*>(index.internalPointer()) : m_root.get();
}

QModelIndex PresetItemModel::indexOf(const Node* n) const
{
  if(!n || n == m_root.get())
    return {};
  return createIndex(n->row, 0, const_cast<Node*>(n));
}

QModelIndex PresetItemModel::index(int row, int column, const QModelIndex& parent) const
{
  auto* p = node(parent);
  if(column != 0 || !ossia::valid_index(row, p->children))
    return {};
  return createIndex(row, 0, p->children[std::size_t(row)].get());
}

QModelIndex PresetItemModel::parent(const QModelIndex& child) const
{
  if(!child.isValid())
    return {};
  return indexOf(node(child)->parent);
}

int PresetItemModel::rowCount(const QModelIndex& parent) const
{
  return int(node(parent)->children.size());
}

int PresetItemModel::columnCount(const QModelIndex& parent) const
{
  return 1;
}

int PresetItemModel::presetRow(const QModelIndex& index) const
{
  if(!index.isValid())
    return -1;
  const int row = node(index)->preset;
  return ossia::valid_index(row, presets) ? row : -1;
}

const Process::Preset* PresetItemModel::preset(const QModelIndex& index) const
{
  if(const int row = presetRow(index); row >= 0)
    return &presets[row];
  if(index.isValid())
    if(const int b = node(index)->builtin; ossia::valid_index(b, m_builtins))
      return &m_builtins[b];
  return nullptr;
}

QModelIndex PresetItemModel::indexOfPreset(int presetRow) const
{
  const auto find = [&](auto& self, const Node& n) -> const Node* {
    for(auto& c : n.children)
    {
      if(c->preset == presetRow)
        return c.get();
      if(auto found = self(self, *c))
        return found;
    }
    return nullptr;
  };
  return presetRow >= 0 ? indexOf(find(find, *m_root)) : QModelIndex{};
}

int PresetItemModel::presetRowOf(const QString& path) const
{
  if(path.isEmpty())
    return -1;
  const auto it = ossia::find_if(presets, [&](const auto& p) { return p.path == path; });
  return it == presets.end() ? -1 : int(it - presets.begin());
}

QStringList PresetItemModel::folderPath(const QModelIndex& index) const
{
  QStringList path;
  for(auto* n = node(index); n && n != m_root.get(); n = n->parent)
    if(n->preset < 0 && n->builtin < 0)
      path.prepend(n->name);
  return path;
}

QModelIndex PresetItemModel::indexOfFolder(const QStringList& path) const
{
  const Node* n = m_root.get();
  for(const auto& part : path)
  {
    auto it = std::find_if(n->children.begin(), n->children.end(), [&](auto& c) {
      return c->preset < 0 && c->builtin < 0 && c->name == part;
    });
    if(it == n->children.end())
      return {};
    n = it->get();
  }
  return indexOf(n);
}

QVariant PresetItemModel::data(const QModelIndex& index, int role) const
{
  if(!index.isValid())
    return {};
  const auto* n = node(index);
  switch(role)
  {
    case Qt::DisplayRole:
    case Qt::EditRole:
      return n->name;
    case Qt::FontRole:
      if(auto p = preset(index); p && m_target && Process::isCurrentPreset(*m_target, *p))
      {
        QFont f;
        f.setBold(true);
        return f;
      }
      break;
    case Qt::ToolTipRole:
      if(auto p = preset(index); p && !p->description.isEmpty())
        return p->description;
      break;
  }
  return {};
}

Qt::ItemFlags PresetItemModel::flags(const QModelIndex& index) const
{
  Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsDropEnabled;
  if(!index.isValid())
    return f;
  f |= Qt::ItemIsSelectable;
  if(preset(index))
    f |= Qt::ItemIsDragEnabled;
  if(const int row = presetRow(index); row >= 0 && canEdit(row))
    f |= Qt::ItemIsEditable;
  return f;
}

static bool isValidForFilename(const QString& name)
{
  if(name.isEmpty())
    return false;

  for(QChar c : QStringLiteral(",^@=+{}[]~!?:&*\"|#%<>$\"';`'/\\"))
    if(name.contains(c))
      return false;

  return true;
}

//! Writes the preset to `path`, made unique, under the name of the file. The
//! file `replaced`, if `path` is it (a rename that only changes the case of
//! its name on a file system that ignores it), is written over.
static bool writePresetFile(
    Process::Preset& preset, const QString& path, const QString& replaced = {})
{
  const bool same = !replaced.isEmpty() && QFileInfo::exists(path)
                    && QFileInfo{path}.canonicalFilePath()
                           == QFileInfo{replaced}.canonicalFilePath();
  const QString unique = same ? path : score::addUniqueSuffix(path);
  Process::Preset written = preset;
  written.name = QFileInfo{unique}.completeBaseName();
  written.path = unique;

  if(!writePresetData(unique, written.toJson()))
    return false;
  preset = std::move(written);
  return true;
}

//! A new preset goes in the user folder, under its process (and plug-in)
static bool writeNewPreset(Process::Preset& preset)
{
  const auto& ctx = score::GUIAppContext();

  const auto& procs = ctx.interfaces<Process::ProcessFactoryList>();
  auto* f = procs.get(preset.key.key);
  if(!f)
    return false;
  auto& factory = *f;
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
  if(role != Qt::EditRole)
    return false;
  const int row = presetRow(index);
  if(!canEdit(row))
    return false;
  auto str = value.toString();
  if(!isValidForFilename(str))
    return false;

  auto& preset = presets[row];
  if(str == preset.name)
    return true;

  // In its own folder, wherever that is in the user library
  const QString old = preset.path;
  auto renamed = preset;
  renamed.name = str;
  if(!writePresetFile(renamed, QFileInfo{old}.absolutePath() + "/" + str + ".scp", old))
    return false;
  // Only once the new file is there, and unless it is the same one
  if(QFileInfo{renamed.path}.canonicalFilePath() != QFileInfo{old}.canonicalFilePath())
    QFile::remove(old);
  preset = std::move(renamed);
  m_plugin->presetChanged(row);
  this->renamed(indexOfPreset(row));
  return true;
}

bool PresetItemModel::dropMimeData(
    const QMimeData* data, Qt::DropAction act, int row, int col,
    const QModelIndex& parent)
{
  // One of its presets, dragged to another folder
  if(!data->hasFormat(score::mime::layerdata()))
  {
    const bool ok = canDropMimeData(data, act, row, col, parent);
    const int dragged = presetRowOf(std::exchange(m_dragged, {}));
    return ok && movePreset(dragged, folderPath(parent).join('/'));
  }

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

  // Into the folder it is dropped on, or beside the preset
  return savePreset(*proc, folderPath(parent).join('/'));
}

bool PresetItemModel::savePreset(const Process::ProcessModel& proc, const QString& category)
{
  auto preset = proc.savePreset();
  preset.category = category;
  if(!writeNewPreset(preset))
    return false;

  m_plugin->addPreset(std::move(preset));
  return true;
}

bool PresetItemModel::canDropMimeData(
    const QMimeData* data, Qt::DropAction act, int row, int col,
    const QModelIndex& parent) const
{
  if(isBuiltin(parent))
    return false;
  if(data->hasFormat(score::mime::layerdata()))
    return true;
  // The preset dragged out of this view, if that is what comes back: its
  // file says which, as the rows can have moved since
  if(m_dragged.isEmpty() || !data->hasFormat(presetFileMime))
    return false;
  if(QString::fromUtf8(data->data(presetFileMime)) != m_dragged)
    return false;
  return canEdit(presetRowOf(m_dragged));
}

QMimeData* PresetItemModel::mimeData(const QModelIndexList& indexes) const
{
  if(indexes.empty())
    return nullptr;
  auto p = preset(indexes.front());
  if(!p)
    return nullptr;

  auto mime = new QMimeData;
  mime->setData(score::mime::processpreset(), p->toJson());
  m_dragged = p->path;
  if(!m_dragged.isEmpty())
    mime->setData(presetFileMime, m_dragged.toUtf8());
  return mime;
}

Qt::DropActions PresetItemModel::supportedDragActions() const
{
  return Qt::CopyAction | Qt::MoveAction;
}

Qt::DropActions PresetItemModel::supportedDropActions() const
{
  return Qt::MoveAction | Qt::CopyAction | Qt::LinkAction | Qt::IgnoreAction;
}
}
