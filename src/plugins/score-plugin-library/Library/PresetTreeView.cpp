#include "PresetTreeView.hpp"

#include <Library/PresetItemModel.hpp>

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QTimer>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Library::PresetTreeView)

namespace Library
{

PresetTreeView::PresetTreeView(QWidget* parent)
    : QTreeView{parent}
{
  setHeaderHidden(true);
  setUniformRowHeights(true);
  setAnimated(true);
  setExpandsOnDoubleClick(false);
  setEditTriggers(QAbstractItemView::EditKeyPressed);
  setDragEnabled(true);
  setAcceptDrops(true);
  setDropIndicatorShown(true);
  setDragDropMode(QAbstractItemView::DragDrop);
  setDefaultDropAction(Qt::CopyAction);

  // What is open is remembered while the user opens and closes it, not what a
  // search opened
  connect(this, &QTreeView::expanded, this, [this](const QModelIndex& idx) {
    if(!m_restoring && m_model && m_model->filterText().isEmpty())
      m_expanded[expansionKey()].insert(m_model->folderPath(idx).join('/'));
  });
  connect(this, &QTreeView::collapsed, this, [this](const QModelIndex& idx) {
    if(!m_restoring && m_model && m_model->filterText().isEmpty())
      m_expanded[expansionKey()].remove(m_model->folderPath(idx).join('/'));
  });
}

PresetTreeView::~PresetTreeView() = default;

void PresetTreeView::setPresetModel(PresetItemModel* model)
{
  m_model = model;
  setModel(model);
  connect(model, &QAbstractItemModel::modelAboutToBeReset, this, [this] { saveState(); });
  connect(model, &QAbstractItemModel::modelReset, this, [this] { restoreState(); });
  connect(model, &PresetItemModel::renamed, this, [this](const QModelIndex& idx) {
    m_restoring = true;
    setCurrentIndex(idx);
    scrollTo(idx);
    m_restoring = false;
  });
}

QString PresetTreeView::expansionKey() const
{
  if(!m_model || !m_model->process())
    return {};
  return QString::fromLatin1(score::uuids::toByteArray(m_model->process()->key.impl()))
         + "/" + m_model->process()->effect;
}

void PresetTreeView::saveState()
{
  m_currentPath.clear();
  m_currentName.clear();
  m_currentCategory.clear();
  if(auto p = m_model->preset(currentIndex()))
  {
    m_currentPath = p->path;
    m_currentName = p->name;
    m_currentCategory = p->category;
  }
}

void PresetTreeView::restoreState()
{
  // The model rebuilt the same presets, another process's, or a search: none
  // of this is the user choosing a preset
  m_restoring = true;
  if(!m_model->filterText().isEmpty())
  {
    expandAll();
  }
  else if(auto it = m_expanded.find(expansionKey()); it != m_expanded.end())
  {
    for(const QString& folder : it->second)
      if(auto idx = m_model->indexOfFolder(folder.split('/')); idx.isValid())
        expand(idx);
  }

  QModelIndex current;
  if(!m_currentName.isEmpty())
  {
    for(int row = 0; row < int(m_model->presets.size()) && !current.isValid(); row++)
    {
      const auto& p = m_model->presets[row];
      const bool same = m_currentPath.isEmpty()
                            ? p.name == m_currentName && p.category == m_currentCategory
                            : p.path == m_currentPath;
      if(same)
        current = m_model->indexOfPreset(row);
    }
  }
  if(current.isValid())
    setCurrentIndex(current);
  m_restoring = false;
  if(current.isValid())
    scrollToCurrentLater();
}

void PresetTreeView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
  QTreeView::currentChanged(current, previous);
  if(m_restoring || !m_model)
    return;
  if(auto p = m_model->preset(current))
    selected(*p);
  else
    selected(std::nullopt);
}

void PresetTreeView::mouseDoubleClickEvent(QMouseEvent* event)
{
  const auto idx = indexAt(event->pos());
  if(auto p = m_model ? m_model->preset(idx) : nullptr)
  {
    if(m_model->target())
      loadRequested(*p);
    else
      doubleClicked(*p);
    event->accept();
    return;
  }
  if(idx.isValid())
    setExpanded(idx, !isExpanded(idx));
  event->accept();
}

void PresetTreeView::paintEvent(QPaintEvent* e)
{
  QTreeView::paintEvent(e);

  if(model() && model()->rowCount(rootIndex()) > 0)
    return;

  QPainter p{this->viewport()};
  p.drawText(rect(), Qt::AlignLeft, "<No presets>");
}

void PresetTreeView::contextMenuEvent(QContextMenuEvent* event)
{
  const auto idx = indexAt(event->pos());
  const int row = m_model ? m_model->presetRow(idx) : -1;
  if(row < 0)
    return;

  QMenu menu;
  const bool editable = m_model->canEdit(row);
  auto rename = menu.addAction(tr("Rename"));
  rename->setEnabled(editable);
  connect(rename, &QAction::triggered, this, [this, idx] { edit(idx); });
  auto remove = menu.addAction(tr("Delete"));
  remove->setEnabled(editable);
  connect(remove, &QAction::triggered, this, [this, row] { m_model->removePreset(row); });
  if(!editable)
    menu.addAction(tr("Presets of installed packages cannot be changed"))
        ->setEnabled(false);
  menu.exec(event->globalPos());
  event->accept();
}

void PresetTreeView::showCurrentPreset()
{
  if(!m_model)
    return;
  const auto idx = m_model->currentPresetIndex();
  if(!idx.isValid())
    return;
  m_restoring = true;
  for(auto p = idx.parent(); p.isValid(); p = p.parent())
    expand(p);
  setCurrentIndex(idx);
  m_restoring = false;
  scrollToCurrentLater();
}

void PresetTreeView::scrollToCurrentLater()
{
  // Once the rows just opened are laid out
  QTimer::singleShot(0, this, [this] {
    scrollTo(currentIndex(), QAbstractItemView::PositionAtCenter);
  });
}

void PresetTreeView::keyPressEvent(QKeyEvent* event)
{
  if(m_model && m_model->target() && state() != QAbstractItemView::EditingState
     && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter))
  {
    if(auto p = m_model->preset(currentIndex()))
    {
      loadRequested(*p);
      event->accept();
      return;
    }
  }
  if(m_model && event->key() == Qt::Key_Delete
     && state() != QAbstractItemView::EditingState)
  {
    if(const int row = m_model->presetRow(currentIndex()); row >= 0)
    {
      m_model->removePreset(row);
      event->accept();
      return;
    }
  }
  QTreeView::keyPressEvent(event);
}
}
