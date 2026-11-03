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
#include <Dataflow/MappingPanel/ConnectionsCommands.hpp>

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
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QKeyEvent>
#include <QSortFilterProxyModel>
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
  m_kinds->setToolTip(tr("Kind of connections to show"));
  m_grouping = new QComboBox{bar};
  m_grouping->addItems(
      {tr("No grouping"), tr("Group by kind"), tr("Group by source"),
       tr("Group by target"), tr("Group by device")});
  m_grouping->setToolTip(tr("How to group the connections"));
  barLay->addWidget(m_search, 1);
  barLay->addWidget(m_kinds);
  barLay->addWidget(m_grouping);
  lay->addWidget(bar);

  m_model = new QStandardItemModel{this};
  m_proxy = new QSortFilterProxyModel{this};
  m_proxy->setSourceModel(m_model);
  m_proxy->setRecursiveFilteringEnabled(true);
  m_proxy->setFilterKeyColumn(-1);
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

  m_summary = new QLabel{this};
  lay->addWidget(m_summary);

  connect(m_search, &QLineEdit::textChanged, this, [this](const QString& txt) {
    m_proxy->setFilterFixedString(txt);
    m_view->expandAll();
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
  delete m_tracking;
  m_tracking = nullptr;
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

  // Creating or removing processes anywhere in the hierarchy goes through a
  // command, as do undo and redo
  m_documentConnections.push_back(connect(
      &m_ctx->document.commandStack(), &score::CommandStack::indexChanged, this,
      [this] { schedule(); }));

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
  if(obj == m_view
     && (ev->type() == QEvent::KeyPress || ev->type() == QEvent::ShortcutOverride))
  {
    auto key = static_cast<QKeyEvent*>(ev);
    if(key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace)
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
}

void ConnectionsWidget::track()
{
  delete m_tracking;
  m_tracking = new QObject{this};
  if(!m_watchedScenario)
    return;

  auto changed = [this] { schedule(); };
  auto ctx = m_tracking;
  for(auto& cable : static_cast<Scenario::ScenarioDocumentModel&>(*m_watchedScenario).cables)
    connect(&cable, &Process::Cable::typeChanged, ctx, changed);

  for(auto proc : m_watchedScenario->findChildren<Process::ProcessModel*>())
  {
    connect(proc, &Process::ProcessModel::inletsChanged, ctx, changed);
    connect(proc, &Process::ProcessModel::outletsChanged, ctx, changed);
    connect(&proc->metadata(), &score::ModelMetadata::NameChanged, ctx, changed);
    auto watchPort = [&](Process::Port* port) {
      connect(port, &Process::Port::addressChanged, ctx, changed);
      connect(port, &Process::Port::nameChanged, ctx, changed);
      connect(port, &Process::Port::cablesChanged, ctx, changed);
    };
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
  m_model->clear();
  m_model->setHorizontalHeaderLabels(
      {tr("Kind"), tr("From"), tr("To"), tr("Type"), tr("Mode"), tr("Location")});

  const auto grouping = Grouping(m_grouping->currentIndex());
  const auto kinds = KindFilter(m_kinds->currentIndex());
  ossia::hash_map<QString, QStandardItem*> groups;
  ossia::hash_map<QStandardItem*, int> groupCounts;
  auto& skin = score::Skin::instance();

  int cables = 0, addresses = 0;
  for(int i = 0, n = std::ssize(m_rows); i < n; i++)
  {
    const auto& r = m_rows[i];
    const bool isCable = r.kind == ConnectionRow::Cable;
    if((kinds == CablesOnly && !isCable) || (kinds == AddressesOnly && isCable))
      continue;
    (isCable ? cables : addresses)++;

    QList<QStandardItem*> items;
    items.reserve(ColumnCount);
    items.push_back(new QStandardItem{isCable ? tr("Cable") : tr("Address")});
    items.push_back(new QStandardItem{r.from});
    items.push_back(new QStandardItem{r.to});
    items.push_back(new QStandardItem{portTypeName(r.portType)});
    items.push_back(new QStandardItem{r.mode});
    items.push_back(new QStandardItem{r.location});
    items[KindColumn]->setData(i, RowRole);
    items[TypeColumn]->setData(
        Process::portBrush(r.portType).color(), Qt::DecorationRole);
    items[LocationColumn]->setToolTip(r.location);
    if(!isCable)
    {
      const QString dir = r.addressIsSource ? tr("The address feeds the port")
                                            : tr("The port writes to the address");
      items[KindColumn]->setToolTip(dir);
      if(r.status != ConnectionRow::Resolved)
      {
        const QBrush warn = skin.Warn1.color();
        for(auto it : items)
          it->setForeground(warn);
        items[ModeColumn]->setToolTip(
            r.status == ConnectionRow::Pattern
                ? tr("The address is a pattern, it may match several nodes")
                : tr("The device explorer does not have this address"));
      }
    }
    for(auto it : items)
      it->setEditable(false);

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
        QList<QStandardItem*> row{g};
        for(int c = 1; c < ColumnCount; c++)
        {
          auto filler = new QStandardItem;
          filler->setEditable(false);
          row.push_back(filler);
        }
        m_model->appendRow(row);
      }
      g->appendRow(items);
      groupCounts[g]++;
    }
  }
  for(auto& [g, count] : groupCounts)
    g->setText(QStringLiteral("%1 (%2)").arg(g->text()).arg(count));

  m_view->expandAll();
  for(int c = 0; c < ColumnCount - 1; c++)
    m_view->resizeColumnToContents(c);

  m_summary->setText(
      alive() ? tr("%1 cable(s), %2 address binding(s)").arg(cables).arg(addresses)
            : tr("No document"));

  if(!selected.empty())
  {
    m_syncingSelection = true;
    QItemSelection sel;
    for(int i = 0, n = std::ssize(m_rows); i < n; i++)
      if(auto obj = selectable(m_rows[i]); obj && selected.contains(obj))
        if(auto idx = viewIndexOf(i); idx.isValid())
          sel.select(idx, idx.siblingAtColumn(ColumnCount - 1));
    m_view->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
    m_syncingSelection = false;
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
  const auto matches = m_model->match(
      m_model->index(0, KindColumn), RowRole, row, 1,
      Qt::MatchExactly | Qt::MatchRecursive);
  if(matches.isEmpty())
    return {};
  return m_proxy->mapFromSource(matches.front());
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
  if(m_syncingSelection)
    return;
  selectInDocument(selectedRows());
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
  m_syncingSelection = true;
  QItemSelection sel;
  for(int i = 0, n = std::ssize(m_rows); i < n; i++)
  {
    auto obj = selectable(m_rows[i]);
    if(obj && s.contains(obj))
      if(auto idx = viewIndexOf(i); idx.isValid())
        sel.select(idx, idx.siblingAtColumn(ColumnCount - 1));
  }
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
  if(r.sourceProcess)
    objects.push_back(r.sourceProcess);
  if(r.sinkProcess)
    objects.push_back(r.sinkProcess);
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
            n && &n->model() == r.sourceProcess.data())
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
    RedoMacroCommandDispatcher<RemoveConnections> macro{m_ctx->commandStack};
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
  CommandDispatcher<> disp{m_ctx->commandStack};
  for(const auto& r : rows)
  {
    if(r.cable && r.cable->type() != t)
      disp.submit<Dataflow::UpdateCable>(*r.cable, t);
  }
}

void ConnectionsWidget::contextMenu(const QPoint& pos)
{
  const int clicked = rowAt(m_view->indexAt(pos));
  if(clicked < 0)
    return;
  // Copies: the menu runs an event loop during which the list may be rebuilt
  const auto rows = rowsAt(selectedRows());
  if(rows.empty())
    return;
  const ConnectionRow clickedRow = m_rows[clicked];

  bool anyCable = false, anyAddress = false;
  for(const auto& r : rows)
    (r.kind == ConnectionRow::Cable ? anyCable : anyAddress) = true;

  QMenu menu{this};
  auto show = menu.addAction(tr("Show in view"));
  connect(show, &QAction::triggered, this, [this, clickedRow] { reveal(clickedRow); });

  if(anyCable)
  {
    auto modes = menu.addMenu(tr("Cable mode"));
    const auto& r = clickedRow;
    for(int t = 0; t <= int(Process::CableType::DelayedStrict); t++)
    {
      auto act = modes->addAction(cableTypeName(t));
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
    connect(copy, &QAction::triggered, this, [this, rows] {
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
  remove->setShortcut(QKeySequence::Delete);
  connect(remove, &QAction::triggered, this, [this, rows] { removeConnections(rows); });

  menu.exec(m_view->viewport()->mapToGlobal(pos));
}
}
