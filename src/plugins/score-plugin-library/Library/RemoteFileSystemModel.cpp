#include "RemoteFileSystemModel.hpp"

#include <score/widgets/IconProvider.hpp>

#include <QHeaderView>
#include <QMimeData>
#include <QPointer>


namespace Library
{
RemoteFileSystemModel::RemoteFileSystemModel(EnvironmentSource env, QObject* parent)
    : QAbstractItemModel{parent}
    , m_env{std::move(env)}
    , m_root{std::make_unique<Entry>()}
{
  m_root->directory = true;
}

RemoteFileSystemModel::~RemoteFileSystemModel() = default;

void RemoteFileSystemModel::setRoot(const score::Uri& uri)
{
  beginResetModel();
  m_root = std::make_unique<Entry>();
  m_root->uri = uri;
  m_root->directory = true;
  endResetModel();
}

RemoteFileSystemModel::Entry* RemoteFileSystemModel::entryOf(const QModelIndex& idx) const
{
  if(!idx.isValid())
    return m_root.get();
  return static_cast<Entry*>(idx.internalPointer());
}

QModelIndex RemoteFileSystemModel::indexOf(Entry& e) const
{
  if(!e.parent)
    return {};

  auto& siblings = e.parent->children;
  for(std::size_t i = 0; i < siblings.size(); i++)
    if(siblings[i].get() == &e)
      return createIndex((int)i, 0, &e);

  return {};
}

score::Uri RemoteFileSystemModel::uriAt(const QModelIndex& index) const
{
  auto* e = entryOf(index);
  return e ? e->uri : score::Uri{};
}

bool RemoteFileSystemModel::isDirectory(const QModelIndex& index) const
{
  auto* e = entryOf(index);
  return e && e->directory;
}

QModelIndex
RemoteFileSystemModel::index(int row, int column, const QModelIndex& parent) const
{
  auto* p = entryOf(parent);
  if(!p || row < 0 || row >= (int)p->children.size() || column != 0)
    return {};

  return createIndex(row, column, p->children[row].get());
}

QModelIndex RemoteFileSystemModel::parent(const QModelIndex& index) const
{
  auto* e = entryOf(index);
  if(!e || !e->parent || e->parent == m_root.get())
    return {};

  return indexOf(*e->parent);
}

int RemoteFileSystemModel::rowCount(const QModelIndex& parent) const
{
  auto* p = entryOf(parent);
  return p ? (int)p->children.size() : 0;
}

int RemoteFileSystemModel::columnCount(const QModelIndex&) const
{
  return 1;
}

QVariant RemoteFileSystemModel::data(const QModelIndex& index, int role) const
{
  auto* e = entryOf(index);
  if(!e || e == m_root.get())
    return {};

  switch(role)
  {
    case Qt::DisplayRole:
      return e->name;
    case Qt::ToolTipRole:
      return e->uri.toString();
    case Qt::DecorationRole:
      return score::IconProvider::instance().icon(
          e->directory ? QFileIconProvider::Folder : QFileIconProvider::File);
    default:
      return {};
  }
}

QVariant
RemoteFileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if(orientation == Qt::Horizontal && role == Qt::DisplayRole && section == 0)
    return tr("Name");
  return {};
}

Qt::ItemFlags RemoteFileSystemModel::flags(const QModelIndex& index) const
{
  if(!index.isValid())
    return Qt::NoItemFlags;

  auto f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  if(!isDirectory(index))
    f |= Qt::ItemIsDragEnabled;
  return f;
}

bool RemoteFileSystemModel::hasChildren(const QModelIndex& parent) const
{
  auto* p = entryOf(parent);
  if(!p)
    return false;

  // A folder we have not opened is assumed to have something in it, or the view
  // shows no arrow and there is no way to ask.
  return p->directory && (!p->listed || !p->children.empty());
}

bool RemoteFileSystemModel::canFetchMore(const QModelIndex& parent) const
{
  auto* p = entryOf(parent);
  return p && p->directory && !p->requested;
}

void RemoteFileSystemModel::fetchMore(const QModelIndex& parent)
{
  auto* p = entryOf(parent);
  if(!p || !p->directory || p->requested)
    return;

  p->requested = true;

  // The answer comes back later and this model may be gone by then -- the
  // document it belongs to can be closed while a listing is in flight.
  QPointer<RemoteFileSystemModel> self = this;
  Entry* target = p;

  auto* env = m_env ? m_env() : nullptr;
  if(!env)
    return;

  env->list(
      p->uri,
      [self, target](std::vector<score::DirEntry> entries) {
    if(!self)
      return;

    // Folders first, then by name, as a file browser shows them.
    std::sort(
        entries.begin(), entries.end(),
        [](const score::DirEntry& a, const score::DirEntry& b) {
      if(a.directory != b.directory)
        return a.directory;
      return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });

    const auto idx = self->indexOf(*target);
    self->beginInsertRows(idx, 0, (int)entries.size() - 1);
    for(auto& de : entries)
    {
      auto child = std::make_unique<Entry>();
      child->uri = de.uri;
      child->name = de.name;
      child->directory = de.directory;
      child->size = de.size;
      child->parent = target;
      target->children.push_back(std::move(child));
    }
    target->listed = true;
    self->endInsertRows();
      },
      [self, target](const QString& err) {
    if(!self)
      return;

    // Marked listed with nothing in it: an unreadable folder is empty as far as
    // anyone here can tell, and asking again on every repaint helps nobody.
    target->listed = true;
    qDebug() << "Could not list a folder on the other machine:" << err;
      });
}

QStringList RemoteFileSystemModel::mimeTypes() const
{
  return {score::remoteUriMimeType()};
}

QMimeData* RemoteFileSystemModel::mimeData(const QModelIndexList& indexes) const
{
  // Not text/uri-list with file:// URLs, which is what a local file browser
  // hands over. These files are not on this machine: a local path would name
  // something that is not there, and every existing drop handler would try to
  // open it. A type of its own is ignored by handlers that do not know it,
  // which is the correct outcome until one does.
  QStringList uris;
  for(const auto& idx : indexes)
  {
    if(!idx.isValid() || idx.column() != 0 || isDirectory(idx))
      continue;

    uris.push_back(uriAt(idx).toString());
  }

  if(uris.empty())
    return nullptr;

  auto* mime = new QMimeData;
  mime->setData(score::remoteUriMimeType(), uris.join('\n').toUtf8());
  mime->setText(uris.join('\n'));
  return mime;
}
}

namespace Library
{
RemoteLibraryWidget::RemoteLibraryWidget(QWidget* parent)
    : QTreeView{parent}
{
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setAlternatingRowColors(true);
  header()->hide();
}

RemoteLibraryWidget::~RemoteLibraryWidget() = default;

void RemoteLibraryWidget::browse(
    RemoteFileSystemModel::EnvironmentSource env, const score::Uri& root)
{
  // A fresh model per document: the old one's entries name folders on a
  // machine we may no longer be talking to.
  auto* old = m_model;
  m_model = new RemoteFileSystemModel{std::move(env), this};
  setModel(m_model);
  delete old;

  m_model->setRoot(root);
}

void RemoteLibraryWidget::clear()
{
  setModel(nullptr);
  delete m_model;
  m_model = nullptr;
}
}
