#include "ConnectionsWidget.hpp"

#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/CableItem.hpp>
#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/Skin.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/widgets/MarginLess.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentView.hpp>

#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/small_vector.hpp>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QLineEdit>
#include <QScrollBar>
#include <QMenu>
#include <QKeyEvent>
#include <QSortFilterProxyModel>
#include <QStyle>
#include <QStandardItemModel>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

namespace Dataflow
{
namespace
{
//! The intervals above obj, outermost first
ossia::small_vector<Scenario::IntervalModel*, 8> intervalsAbove(const QObject* obj)
{
  ossia::small_vector<Scenario::IntervalModel*, 8> res;
  for(auto p = obj ? obj->parent() : nullptr; p; p = p->parent())
    if(auto itv = qobject_cast<Scenario::IntervalModel*>(p))
      res.push_back(itv);
  std::reverse(res.begin(), res.end());
  return res;
}

//! A square of the color of a port type, inset in an icon so that the
//! squares of consecutive rows stay apart whatever the row height
QIcon typeIndicator(Process::PortType t)
{
  QPixmap pix{16, 16};
  pix.fill(Qt::transparent);
  {
    QPainter p{&pix};
    p.fillRect(QRect{3, 3, 10, 10}, Process::portBrush(t).color());
  }
  return QIcon{pix};
}

bool isAncestor(const QObject* ancestor, const QObject* obj)
{
  for(auto p = obj; p; p = p->parent())
    if(p == ancestor)
      return true;
  return false;
}

//! What the document selects for a row: the cable, or the bound port
const IdentifiedObjectAbstract* selectable(const ConnectionRow& r)
{
  if(r.kind == ConnectionRow::Cable)
    return r.cable.data();
  return r.sourcePort.data();
}
}

ConnectionsWidget::ConnectionsWidget(QWidget* parent)
    : QWidget{parent}
{
  auto lay = new score::MarginLess<QVBoxLayout>{this};

  auto bar = new QWidget{this};
  auto barLay = new score::MarginLess<QHBoxLayout>{bar};
  m_search = new QLineEdit{bar};
  m_search->setPlaceholderText(tr("Search"));
  m_search->setClearButtonEnabled(true);
  m_kinds = new QComboBox{bar};
  m_kinds->addItems({tr("All"), tr("Cables"), tr("Addresses")});
  m_grouping = new QComboBox{bar};
  m_grouping->addItems(
      {tr("No grouping"), tr("Group by kind"), tr("Group by source"),
       tr("Group by target"), tr("Group by device")});
  barLay->addWidget(m_search, 1);
  barLay->addWidget(m_kinds);
  barLay->addWidget(m_grouping);
  lay->addWidget(bar);

  m_model = new QStandardItemModel{this};
  m_proxy = new QSortFilterProxyModel{this};
  m_proxy->setSourceModel(m_model);
  m_proxy->setRecursiveFilteringEnabled(true);
  m_proxy->setFilterKeyColumn(-1);
  m_proxy->setFilterRole(SearchRole);
  m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
  m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

  m_view = new QTreeView{this};
  m_view->setModel(m_proxy);
  m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_view->setUniformRowHeights(true);
  m_view->setAlternatingRowColors(true);
  m_view->setSortingEnabled(true);
  m_view->sortByColumn(-1, Qt::AscendingOrder);
  m_view->setContextMenuPolicy(Qt::CustomContextMenu);
  m_view->header()->setStretchLastSection(true);
  lay->addWidget(m_view, 1);

  connect(m_search, &QLineEdit::textChanged, this, [this](const QString& txt) {
    m_proxy->setFilterFixedString(txt);
    m_view->expandAll();
    updateCounts();
  });
  connect(m_kinds, &QComboBox::currentIndexChanged, this, [this] { populate(); });
  connect(m_grouping, &QComboBox::currentIndexChanged, this, [this] { populate(); });
  connect(
      m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
      [this] { onViewSelectionChanged(); });
  connect(m_view, &QTreeView::customContextMenuRequested, this, [this](QPoint p) {
    contextMenu(p);
  });

  m_view->installEventFilter(this);

  populate();
}

ConnectionsWidget::~ConnectionsWidget()
{
  unwatchDocument();
}

void ConnectionsWidget::setDocument(const score::DocumentContext* ctx)
{
  unwatchDocument();
  m_ctx = ctx;
  m_document = ctx ? &ctx->document : nullptr;
  watchDocument();

  // What was listed belongs to the previous document. The new one is listed
  // once the event loop runs again: setModel comes while the document is
  // still being set up.
  m_rows.clear();
  populate({});
  schedule();
}

void ConnectionsWidget::watchDocument()
{
  if(!m_ctx)
    return;
  auto doc = score::IDocument::try_get<Scenario::ScenarioDocumentModel>(m_ctx->document);
  if(!doc)
    return;
  m_watchedScenario = doc;
  doc->cables.added.connect<&ConnectionsWidget::on_cableAdded>(this);
  doc->cables.removed.connect<&ConnectionsWidget::on_cableRemoved>(this);

  // Processes and intervals created or removed anywhere in the hierarchy are
  // seen as children of the tracked intervals and processes, see track()

  if(auto devices = m_ctx->findPlugin<Explorer::DeviceDocumentPlugin>())
  {
    auto& explorer = devices->explorer();
    auto changed = [this] { schedule(); };
    m_documentConnections.push_back(
        connect(&explorer, &QAbstractItemModel::rowsInserted, this, changed));
    m_documentConnections.push_back(
        connect(&explorer, &QAbstractItemModel::rowsRemoved, this, changed));
    m_documentConnections.push_back(
        connect(&explorer, &QAbstractItemModel::modelReset, this, changed));
    m_documentConnections.push_back(
        connect(&devices->list(), &Device::DeviceList::deviceAdded, this, changed));
    m_documentConnections.push_back(
        connect(&devices->list(), &Device::DeviceList::deviceRemoved, this, changed));
  }
}

void ConnectionsWidget::unwatchDocument()
{
  for(auto& c : m_documentConnections)
    QObject::disconnect(c);
  m_documentConnections.clear();
  if(auto doc = qobject_cast<Scenario::ScenarioDocumentModel*>(m_watchedScenario.data()))
  {
    doc->cables.added.disconnect<&ConnectionsWidget::on_cableAdded>(this);
    doc->cables.removed.disconnect<&ConnectionsWidget::on_cableRemoved>(this);
  }
  m_watchedScenario.clear();
  untrack();
}

void ConnectionsWidget::untrack()
{
  for(auto obj : m_tracked)
    obj->removeEventFilter(this);
  m_tracked.clear();
  delete m_tracking;
  m_tracking = nullptr;
}

void ConnectionsWidget::on_cableAdded(const Process::Cable&)
{
  schedule();
}

void ConnectionsWidget::on_cableRemoved(const Process::Cable&)
{
  schedule();
}

void ConnectionsWidget::schedule()
{
  m_dirty = true;
  if(!isVisible() || m_scheduled)
    return;
  m_scheduled = true;
  QTimer::singleShot(0, this, [this] {
    if(m_scheduled)
      rebuild();
  });
}

bool ConnectionsWidget::eventFilter(QObject* obj, QEvent* ev)
{
  if(obj != m_view)
  {
    // A tracked interval or process gained or lost a child: a process, an
    // interval, a port
    if(ev->type() == QEvent::ChildAdded || ev->type() == QEvent::ChildRemoved)
      schedule();
    return false;
  }

  if(ev->type() == QEvent::KeyPress || ev->type() == QEvent::ShortcutOverride)
  {
    auto key = static_cast<QKeyEvent*>(ev);
    if((key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace)
       && !selectedRows().empty())
    {
      // Overriding keeps the application's own Delete action from removing
      // the document selection instead
      key->accept();
      if(ev->type() == QEvent::KeyPress)
        removeConnections(rowsAt(selectedRows()));
      return true;
    }
  }
  return QWidget::eventFilter(obj, ev);
}

void ConnectionsWidget::showEvent(QShowEvent* ev)
{
  QWidget::showEvent(ev);
  if(m_dirty)
    rebuild();
  if(m_selectionPending && alive())
  {
    m_selectionPending = false;
    setSelection(m_ctx->selectionStack.currentSelection());
  }
}

void ConnectionsWidget::refresh()
{
  rebuild();
}

void ConnectionsWidget::rebuild()
{
  m_scheduled = false;
  m_dirty = false;
  const auto keep = selectedObjects();
  m_rows.clear();
  if(alive())
  {
    if(auto doc = score::IDocument::try_get<Scenario::ScenarioDocumentModel>(
           m_ctx->document);
       doc && !doc->closing())
      m_rows = collectConnections(*doc, *m_ctx);
  }
  track();
  populate(keep);
  if(m_selectionPending && isVisible() && alive())
    setSelection(m_ctx->selectionStack.currentSelection());
}

void ConnectionsWidget::track()
{
  if(!m_watchedScenario)
    return;
  if(!m_tracking)
    m_tracking = new QObject{this};

  auto changed = [this] { schedule(); };
  auto ctx = m_tracking;
  // True the first time an object is seen; it is forgotten when destroyed
  auto add = [&](QObject* obj) {
    if(!m_tracked.insert(obj).second)
      return false;
    connect(obj, &QObject::destroyed, ctx, [this, obj] {
      m_tracked.erase(obj);
      schedule();
    });
    return true;
  };
  auto watchPort = [&](Process::Port* port) {
    if(!port || !add(port))
      return;
    connect(port, &Process::Port::addressChanged, ctx, changed);
    connect(port, &Process::Port::nameChanged, ctx, changed);
    connect(port, &Process::Port::cablesChanged, ctx, changed);
  };

  for(auto& cable : static_cast<Scenario::ScenarioDocumentModel&>(*m_watchedScenario).cables)
    if(add(&cable))
      connect(&cable, &Process::Cable::typeChanged, ctx, changed);

  for(auto itv : m_watchedScenario->findChildren<Scenario::IntervalModel*>())
  {
    if(add(itv))
    {
      itv->installEventFilter(this);
      connect(&itv->metadata(), &score::ModelMetadata::NameChanged, ctx, changed);
    }
    watchPort(itv->inlet.get());
    watchPort(itv->outlet.get());
  }

  for(auto proc : m_watchedScenario->findChildren<Process::ProcessModel*>())
  {
    if(add(proc))
    {
      proc->installEventFilter(this);
      connect(proc, &Process::ProcessModel::inletsChanged, ctx, changed);
      connect(proc, &Process::ProcessModel::outletsChanged, ctx, changed);
      connect(&proc->metadata(), &score::ModelMetadata::NameChanged, ctx, changed);
    }
    for(auto p : proc->inlets())
      watchPort(p);
    for(auto p : proc->outlets())
      watchPort(p);
  }
}

ossia::hash_set<const QObject*> ConnectionsWidget::selectedObjects() const
{
  ossia::hash_set<const QObject*> res;
  for(int row : selectedRows())
    if(auto obj = selectable(m_rows[row]))
      res.insert(obj);
  return res;
}

void ConnectionsWidget::populate()
{
  populate(selectedObjects());
}

void ConnectionsWidget::populate(const ossia::hash_set<const QObject*>& selected)
{
  // Clearing the model empties the view's selection: that is not the user
  // deselecting
  m_syncingSelection = true;

  // What the user collapsed and where the list was scrolled survive a rebuild
  for(const auto& [g, key] : m_groups)
  {
    auto idx = m_proxy->mapFromSource(g->index());
    if(!idx.isValid())
      continue;
    if(m_view->isExpanded(idx))
      m_collapsed.erase(key);
    else
      m_collapsed.insert(key);
  }
  const int vscroll = m_view->verticalScrollBar()->value();
  const int hscroll = m_view->horizontalScrollBar()->value();

  m_model->clear();
  m_groups.clear();
  m_items.assign(m_rows.size(), nullptr);
  m_rowsOf.clear();
  m_model->setHorizontalHeaderLabels(
      {tr("Kind"), QString{}, tr("From"), tr("To"), tr("Location")});
  m_model->setHeaderData(StatusColumn, Qt::Horizontal, tr("Binding"), Qt::ToolTipRole);

  const auto grouping = Grouping(m_grouping->currentIndex());
  const auto kinds = KindFilter(m_kinds->currentIndex());
  ossia::hash_map<QString, QStandardItem*> groups;
  auto& skin = score::Skin::instance();
  ossia::hash_map<int, QIcon> indicators;

  for(int i = 0, n = std::ssize(m_rows); i < n; i++)
  {
    const auto& r = m_rows[i];
    const bool isCable = r.kind == ConnectionRow::Cable;
    if((kinds == CablesOnly && !isCable) || (kinds == AddressesOnly && isCable))
      continue;

    auto item = [](const QString& text) {
      auto it = new QStandardItem{text};
      it->setData(text, SearchRole);
      return it;
    };
    const QString kind = isCable ? tr("Cable") : tr("Address");
    const QString type = portTypeName(r.portType);
    QList<QStandardItem*> items;
    items.reserve(ColumnCount);
    items.push_back(item(kind));
    items.push_back(item({}));
    items.push_back(item(r.from));
    items.push_back(item(r.to));
    items.push_back(item(r.location));
    items[KindColumn]->setData(i, RowRole);
    auto& indicator = indicators[int(r.portType)];
    if(indicator.isNull())
      indicator = typeIndicator(r.portType);
    items[KindColumn]->setIcon(indicator);
    items[KindColumn]->setData(int(r.portType), PortTypeRole);
    items[LocationColumn]->setToolTip(r.location);
    if(isCable)
    {
      items[KindColumn]->setToolTip(QStringLiteral("%1\n%2").arg(type, r.mode));
      items[KindColumn]->setData(
          QStringList{kind, type, r.mode}.join('\n'), SearchRole);
    }
    else
    {
      const QString dir = r.addressIsSource ? tr("The address feeds the port")
                                            : tr("The port writes to the address");
      items[KindColumn]->setToolTip(QStringLiteral("%1\n%2").arg(type, dir));
      items[KindColumn]->setData(QStringList{kind, type}.join('\n'), SearchRole);

      auto& status = *items[StatusColumn];
      switch(r.status)
      {
        case ConnectionRow::Resolved:
          break;
        case ConnectionRow::Pattern:
          status.setIcon(QIcon{QStringLiteral(":/icons/message_information.png")});
          status.setToolTip(tr("The address is a pattern, it may match several nodes"));
          break;
        case ConnectionRow::MissingDevice:
        case ConnectionRow::MissingNode: {
          status.setIcon(QIcon{QStringLiteral(":/icons/message_warning.png")});
          status.setToolTip(
              r.status == ConnectionRow::MissingDevice
                  ? tr("Not bound: the device explorer has no device %1").arg(r.device)
                  : tr("Not bound: the device explorer does not have %1").arg(r.address));
          const QBrush warn = skin.Warn1.color();
          for(auto it : items)
            it->setForeground(warn);
          break;
        }
      }
      if(r.status != ConnectionRow::Resolved)
        status.setData(statusName(r.status), SearchRole);
    }
    for(auto it : items)
      it->setEditable(false);
    m_items[i] = items[KindColumn];
    if(auto obj = selectable(r))
      m_rowsOf[obj].push_back(i);

    QString group;
    switch(grouping)
    {
      case NoGrouping:
        break;
      case ByKind:
        group = isCable ? tr("Cables") : tr("Addresses");
        break;
      case ByFrom:
        group = r.fromGroup;
        break;
      case ByTo:
        group = r.toGroup;
        break;
      case ByDevice:
        group = isCable ? tr("Cables") : r.device;
        break;
    }

    if(grouping == NoGrouping)
    {
      m_model->appendRow(items);
    }
    else
    {
      auto& g = groups[group];
      if(!g)
      {
        g = new QStandardItem{group};
        g->setEditable(false);
        g->setData(group, GroupRole);
        g->setData(group, SearchRole);
        QList<QStandardItem*> row{g};
        for(int c = 1; c < ColumnCount; c++)
        {
          auto filler = new QStandardItem;
          filler->setEditable(false);
          row.push_back(filler);
        }
        m_model->appendRow(row);
        m_groups.emplace_back(g, group);
      }
      g->appendRow(items);
    }
  }

  updateCounts();

  m_view->expandAll();
  for(const auto& [g, key] : m_groups)
    if(m_collapsed.contains(key))
      m_view->collapse(m_proxy->mapFromSource(g->index()));
  for(int c = 0; c < ColumnCount - 1; c++)
    if(c != StatusColumn)
      m_view->resizeColumnToContents(c);
  // Icons only: as wide as one, below the style's smallest section
  {
    auto& header = *m_view->header();
    const int w = m_view->style()->pixelMetric(QStyle::PM_SmallIconSize) + 8;
    header.setMinimumSectionSize(std::min(header.minimumSectionSize(), w));
    header.setSectionResizeMode(StatusColumn, QHeaderView::Fixed);
    header.resizeSection(StatusColumn, w);
  }
  m_view->doItemsLayout();
  m_view->verticalScrollBar()->setValue(vscroll);
  m_view->horizontalScrollBar()->setValue(hscroll);

  if(!selected.empty())
  {
    QItemSelection sel;
    for(auto obj : selected)
      if(auto it = m_rowsOf.find(obj); it != m_rowsOf.end())
        for(int i : it->second)
          if(auto idx = viewIndexOf(i); idx.isValid())
            sel.select(idx, idx.siblingAtColumn(ColumnCount - 1));
    m_view->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
  }
  m_syncingSelection = false;
}

void ConnectionsWidget::updateCounts()
{
  if(!alive())
    return;

  for(const auto& [g, key] : m_groups)
  {
    int shown = 0;
    for(int c = 0; c < g->rowCount(); c++)
      if(m_proxy->mapFromSource(g->child(c)->index()).isValid())
        shown++;
    const QString text = QStringLiteral("%1 (%2)").arg(key).arg(shown);
    if(g->text() != text)
      g->setText(text);
  }
}

int ConnectionsWidget::rowAt(const QModelIndex& viewIndex) const
{
  if(!viewIndex.isValid())
    return -1;
  const auto src = m_proxy->mapToSource(viewIndex.siblingAtColumn(KindColumn));
  const auto v = m_model->data(src, RowRole);
  if(!v.isValid())
    return -1;
  const int row = v.toInt();
  return row >= 0 && row < std::ssize(m_rows) ? row : -1;
}

QModelIndex ConnectionsWidget::viewIndexOf(int row) const
{
  if(row < 0 || row >= std::ssize(m_items) || !m_items[row])
    return {};
  return m_proxy->mapFromSource(m_items[row]->index());
}

std::vector<int> ConnectionsWidget::selectedRows() const
{
  std::vector<int> res;
  for(const auto& idx : m_view->selectionModel()->selectedRows(KindColumn))
    if(int row = rowAt(idx); row >= 0)
      res.push_back(row);
  std::sort(res.begin(), res.end());
  return res;
}

void ConnectionsWidget::onViewSelectionChanged()
{
  if(m_syncingSelection || !alive())
    return;
  const auto rows = selectedRows();
  if(rows.empty())
  {
    m_syncingSelection = true;
    score::SelectionDispatcher{m_ctx->selectionStack}.deselect();
    m_syncingSelection = false;
    return;
  }
  selectInDocument(rows);
}

void ConnectionsWidget::selectInDocument(const std::vector<int>& rows)
{
  if(!alive() || rows.empty())
    return;
  Selection sel;
  for(int row : rows)
    if(auto obj = selectable(m_rows[row]))
      sel.append(obj);
  if(sel.empty())
    return;

  // Revealing may display another interval, which must not drop the selection
  reveal(m_rows[rows.front()]);
  m_syncingSelection = true;
  score::SelectionDispatcher{m_ctx->selectionStack}.select(sel);
  m_syncingSelection = false;
}

std::vector<ConnectionRow> ConnectionsWidget::rowsAt(const std::vector<int>& rows) const
{
  std::vector<ConnectionRow> res;
  res.reserve(rows.size());
  for(int row : rows)
    if(row >= 0 && row < std::ssize(m_rows))
      res.push_back(m_rows[row]);
  return res;
}

void ConnectionsWidget::setSelection(const Selection& s)
{
  if(m_syncingSelection)
    return;
  if(!isVisible() || refreshPending())
  {
    // Mirrored from the document's selection once shown and rebuilt
    m_selectionPending = true;
    return;
  }
  m_selectionPending = false;
  m_syncingSelection = true;
  QItemSelection sel;
  for(const auto& obj : s)
    if(auto it = m_rowsOf.find(obj.data()); it != m_rowsOf.end())
      for(int i : it->second)
        if(auto idx = viewIndexOf(i); idx.isValid())
          sel.select(idx, idx.siblingAtColumn(ColumnCount - 1));
  m_view->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
  m_syncingSelection = false;
}

void ConnectionsWidget::reveal(const ConnectionRow& r)
{
  if(!alive())
    return;
  auto pres = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
      m_ctx->document);
  if(!pres)
    return;

  ossia::small_vector<const QObject*, 2> objects;
  if(r.sourceOwner)
    objects.push_back(r.sourceOwner);
  if(r.sinkOwner)
    objects.push_back(r.sinkOwner);
  if(objects.empty())
    return;

  // Only move to another interval when the current one does not show them
  const auto& displayed = pres->displayedInterval();
  const bool shown = std::all_of(objects.begin(), objects.end(), [&](auto o) {
    return isAncestor(&displayed, o);
  });
  if(!shown)
  {
    // The innermost interval enclosing every end of the connection
    auto common = intervalsAbove(objects.front());
    for(std::size_t i = 1; i < objects.size(); i++)
    {
      const auto other = intervalsAbove(objects[i]);
      std::size_t k = 0;
      while(k < common.size() && k < other.size() && common[k] == other[k])
        k++;
      common.resize(k);
    }
    if(!common.empty())
      pres->setDisplayedInterval(common.back());
  }

  auto docView = m_ctx->document.view();
  if(!docView)
    return;
  auto view = qobject_cast<Scenario::ScenarioDocumentView*>(&docView->viewDelegate());
  if(!view)
    return;

  QGraphicsItem* target{};
  for(auto item : view->scene().items())
  {
    if(r.cable)
    {
      if(auto c = qgraphicsitem_cast<Dataflow::CableItem*>(item);
         c && &c->model() == r.cable.data())
      {
        target = c;
        break;
      }
    }
    else if(auto n = qgraphicsitem_cast<Process::NodeItem*>(item);
            n && r.sourceProcess && &n->model() == r.sourceProcess.data())
    {
      target = n;
      break;
    }
  }
  if(!target || !target->isVisible())
    return;

  auto& gv = view->view();
  const QRectF visible = gv.mapToScene(gv.viewport()->rect()).boundingRect();
  if(!visible.contains(target->sceneBoundingRect()))
    gv.centerOn(target);
}

void ConnectionsWidget::removeConnections(const std::vector<ConnectionRow>& rows)
{
  if(!alive() || rows.empty())
    return;
  auto doc = score::IDocument::try_get<Scenario::ScenarioDocumentModel>(m_ctx->document);
  if(!doc)
    return;

  std::vector<score::Command*> cmds;
  for(const auto& r : rows)
  {
    if(r.kind == ConnectionRow::Cable && r.cable)
      cmds.push_back(new Dataflow::RemoveCable{*doc, *r.cable});
    else if(r.kind == ConnectionRow::Address && r.sourcePort)
      cmds.push_back(new Process::ChangePortAddress{*r.sourcePort, {}});
  }

  if(cmds.size() == 1)
  {
    CommandDispatcher<>{m_ctx->commandStack}.submit(cmds.front());
  }
  else if(!cmds.empty())
  {
    RedoMacroCommandDispatcher<Dataflow::RemoveConnections> macro{m_ctx->commandStack};
    for(auto cmd : cmds)
      macro.submit(cmd);
    macro.commit();
  }
}

void ConnectionsWidget::setCableType(
    const std::vector<ConnectionRow>& rows, Process::CableType t)
{
  if(!alive())
    return;
  std::vector<const Process::Cable*> cables;
  for(const auto& r : rows)
    if(r.cable && r.cable->type() != t)
      cables.push_back(r.cable.data());

  if(cables.size() == 1)
  {
    CommandDispatcher<>{m_ctx->commandStack}.submit<Dataflow::UpdateCable>(
        *cables.front(), t);
  }
  else if(!cables.empty())
  {
    RedoMacroCommandDispatcher<Dataflow::UpdateCables> macro{m_ctx->commandStack};
    for(auto c : cables)
      macro.submit(new Dataflow::UpdateCable{*c, t});
    macro.commit();
  }
}

void ConnectionsWidget::contextMenu(const QPoint& pos)
{
  const int clicked = rowAt(m_view->indexAt(pos));
  if(clicked < 0)
    return;
  QMenu menu{this};
  fillContextMenu(menu, clicked);
  if(!menu.isEmpty())
    menu.exec(m_view->viewport()->mapToGlobal(pos));
}

void ConnectionsWidget::fillContextMenu(QMenu& menu, int clicked)
{
  if(clicked < 0 || clicked >= std::ssize(m_rows))
    return;
  // Copies: the menu runs an event loop during which the list may be rebuilt
  const auto rows = rowsAt(selectedRows());
  if(rows.empty())
    return;
  const ConnectionRow clickedRow = m_rows[clicked];

  bool anyCable = false, anyAddress = false;
  for(const auto& r : rows)
    (r.kind == ConnectionRow::Cable ? anyCable : anyAddress) = true;

  auto show = menu.addAction(tr("Show in view"));
  show->setObjectName(QStringLiteral("ShowInView"));
  connect(show, &QAction::triggered, this, [this, clickedRow] { reveal(clickedRow); });

  if(anyCable)
  {
    auto modes = menu.addMenu(tr("Cable mode"));
    modes->setObjectName(QStringLiteral("CableMode"));
    const auto& r = clickedRow;
    for(int t = 0; t <= int(Process::CableType::DelayedStrict); t++)
    {
      auto act = modes->addAction(cableTypeName(t));
      act->setData(t);
      act->setCheckable(true);
      act->setChecked(r.cable && int(r.cable->type()) == t);
      connect(act, &QAction::triggered, this, [this, rows, t] {
        setCableType(rows, Process::CableType(t));
      });
    }
  }
  if(anyAddress)
  {
    auto copy = menu.addAction(tr("Copy address"));
    copy->setObjectName(QStringLiteral("CopyAddress"));
    connect(copy, &QAction::triggered, this, [rows] {
      QStringList addrs;
      for(const auto& r : rows)
        if(r.kind == ConnectionRow::Address)
          addrs.push_back(r.address);
      QApplication::clipboard()->setText(addrs.join('\n'));
    });
  }
  menu.addSeparator();
  const QString removeText = anyCable && anyAddress ? tr("Remove and unbind")
                             : anyCable             ? tr("Remove cable")
                                                    : tr("Unbind address");
  auto remove = menu.addAction(removeText);
  remove->setObjectName(QStringLiteral("Remove"));
  remove->setShortcut(QKeySequence::Delete);
  connect(remove, &QAction::triggered, this, [this, rows] { removeConnections(rows); });
}
}
