// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/ProcessList.hpp>
#include <QGraphicsScene>
#include <QList>
#include <QApplication>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <Scenario/Document/Interval/SlotHandle.hpp>
#include "TemporalIntervalHeader.hpp"
#include "TemporalIntervalPresenter.hpp"
#include "TemporalIntervalView.hpp"
#include <Process/ProcessContext.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/selection/Selectable.hpp>
#include <score/tools/Todo.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <score/widgets/GraphicsItem.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Application/Menus/ScenarioContextMenuManager.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Explorer/Widgets/AddressAccessorEditWidget.hpp>
#include <QDrag>
#include <QFormLayout>
#include <QGraphicsProxyWidget>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
class QColor;
class QObject;
class QString;

namespace Scenario
{
static std::unordered_map<Process::Cable*, CableItem*> g_cables;
static std::unordered_map<Process::Port*, PortItem*> g_ports;

class PortWidget : public QWidget
{
  public:
    PortWidget(const score::DocumentContext& ctx, Process::Port& p):
      m_edit{ctx.plugin<Explorer::DeviceDocumentPlugin>().explorer(), this}
    {
      auto lay = new QFormLayout{this};
      lay->addRow(tr("Address"), &m_edit);
    }

  private:
    Explorer::AddressAccessorEditWidget m_edit;
};

class PortPanel final
    : public QObject
    , public QGraphicsItem
{
    QRectF m_rect;
    QRectF m_widgrect;
    PortWidget* m_pw{};
    QGraphicsProxyWidget* m_proxy{};
  public:
    PortPanel(const score::DocumentContext& ctx, Process::Port& p, QGraphicsItem* parent):
      QGraphicsItem{parent}
    {
      m_pw = new PortWidget{ctx, p};
      m_proxy = new QGraphicsProxyWidget{this};
      m_proxy->setWidget(m_pw);
      m_proxy->setPos(10, 10);
      connect(m_proxy, &QGraphicsProxyWidget::geometryChanged,
              this, &PortPanel::updateRect);
      updateRect();
    }

    void updateRect()
    {
      prepareGeometryChange();

      m_widgrect = m_proxy->subWidgetRect(m_pw);
      m_rect = m_widgrect.adjusted(0, 0, 30, 20);
    }

    QRectF boundingRect() const override
    {
      return m_rect;
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
      painter->setRenderHint(QPainter::Antialiasing, true);
      painter->setPen(QColor("#1A2024"));
      painter->setBrush(QColor("#1A2024"));

      QPainterPath p;
      p.moveTo(12, 12);
      p.lineTo(0, 22);
      p.lineTo(12, 34);
      p.lineTo(12, 12);
      p.closeSubpath();

      painter->drawPath(p);
      painter->fillPath(p, painter->brush());

      painter->drawRoundedRect(m_rect.adjusted(10, 0, 0, 0), 10, 10);

      painter->setRenderHint(QPainter::Antialiasing, false);
    }
};


CableItem::CableItem(Process::Cable& c, QGraphicsItem* parent):
  QGraphicsItem{parent}
, m_cable{c}
{
  g_cables.insert({&c, this});

  auto src = g_ports.find(c.source());
  if(src != g_ports.end())
  {
    m_p1 = src->second;
    m_p1->cables.push_back(this);
  }
  auto snk = g_ports.find(c.sink());
  if(snk != g_ports.end())
  {
    m_p2 = snk->second;
    m_p2->cables.push_back(this);
  }
  check();
  resize();
}

CableItem::~CableItem()
{
  if(m_p1)
  {
    boost::remove_erase(m_p1->cables, this);
  }
  if(m_p2)
  {
    boost::remove_erase(m_p2->cables, this);
  }

  auto it = g_cables.find(&m_cable);
  if(it != g_cables.end())
    g_cables.erase(it);
}

QRectF CableItem::boundingRect() const
{
  return m_path.boundingRect();
}

void CableItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if(m_p1 && m_p2)
  {
    QPen cablepen;
    cablepen.setColor(QColor("#559999dd"));
    cablepen.setJoinStyle(Qt::PenJoinStyle::RoundJoin);
    cablepen.setWidthF(1.5);

    painter->setPen(cablepen);
    painter->setBrush(Qt::transparent);
    painter->drawPath(m_path);
  }
}

void CableItem::resize()
{
  prepareGeometryChange();

  auto p1 = m_p1->scenePos();
  auto p2 = m_p2->scenePos();

  auto rect = QRectF{p1, p2};
  auto nrect = rect.normalized();
  qDebug() << p1 << p2 << rect << nrect;
  this->setPos(nrect.topLeft());
  nrect.translate(-nrect.topLeft().x(), -nrect.topLeft().y());

  p1 = mapFromScene(p1);
  p2 = mapFromScene(p2);

  qDebug() << p1 << p2 << nrect;
  auto first = p1.x() < p2.x() ? p1 : p2;
  auto last = p1.x() >= p2.x() ? p1 : p2;
  QPainterPath p;
  p.moveTo(first.x(), first.y());
  p.lineTo(first.x(), last.y());
  p.lineTo(last.x(), last.y());
  m_path = p;

  update();
}

void CableItem::check()
{
  if(m_p1 && m_p2 && m_p1->isVisible() && m_p2->isVisible()) {
    qDebug("1");
    if(!isEnabled())
    {
      setVisible(true);
      setEnabled(true);
    }
    resize();
  }
  else if(isEnabled()) {
    qDebug("2");
    setVisible(false);
    setEnabled(false);
  }
}

QPainterPath CableItem::shape() const
{
  return m_path;
}


PortItem::PortItem(Process::Port& p, QGraphicsItem* parent)
  : QGraphicsItem{parent}
  , m_port{p}
{
  this->setAcceptDrops(true);
  this->setAcceptHoverEvents(true);
  this->setFlag(QGraphicsItem::ItemSendsScenePositionChanges, true);

  g_ports.insert({&p, this});

  for(auto c : g_cables)
  {
    if(c.first->source() == &p)
    {
      c.second->setSource(this);
    }
    if(c.first->sink() == &p)
    {
      c.second->setTarget(this);
    }
  }
}

PortItem::~PortItem()
{
  for(auto cable : cables)
  {
    if(cable->source() == this)
      cable->setSource(nullptr);
    if(cable->target() == this)
      cable->setTarget(nullptr);
  }
  auto it = g_ports.find(&m_port);
  if(it != g_ports.end())
    g_ports.erase(it);
}

QRectF PortItem::boundingRect() const
{
  return {-m_diam/2., -m_diam/2., m_diam, m_diam};
}

void PortItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, true);
  QColor c;
  switch(m_port.type)
  {
    case Process::PortType::Audio:
      c = QColor("#FFAAAA");
      break;
    case Process::PortType::Message:
      c = QColor("#AAFFAA");
      break;
    case Process::PortType::Midi:
      c = QColor("#AAAAFF");
      break;
  }

  QPen p = c;
  p.setWidth(2);
  QBrush b = c.darker();

  painter->setPen(p);
  painter->setBrush(b);
  painter->drawEllipse(boundingRect());
  painter->setRenderHint(QPainter::Antialiasing, false);
}

static PortItem* clickedPort{};
void PortItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  clickedPort = this;
  event->accept();
}

void PortItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(QLineF(pos(), event->pos()).length() > QApplication::startDragDistance())
  {
    QDrag d{this};
    QMimeData* m = new QMimeData;
    m->setText("cable");
    d.setMimeData(m);
    d.exec();
  }
}

void PortItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if(this->contains(event->pos()))
    emit showPanel();
  event->accept();
}

void PortItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  prepareGeometryChange();
  m_diam = 8.;
  update();
  event->accept();
}

void PortItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
  event->accept();
}

void PortItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  prepareGeometryChange();
  m_diam = 6.;
  update();
  event->accept();
}

void PortItem::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  prepareGeometryChange();
  m_diam = 8.;
  update();
  event->accept();
}

void PortItem::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
  event->accept();
}

void PortItem::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  prepareGeometryChange();
  m_diam = 6.;
  update();
  event->accept();
}

void PortItem::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  prepareGeometryChange();
  m_diam = 6.;
  update();
  if(this != clickedPort)
  {
    if(this->m_port.outlet != clickedPort->m_port.outlet)
    {
      emit createCable(clickedPort, this);
    }
  }
  clickedPort = nullptr;
  event->accept();
}

QVariant PortItem::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
  switch(change)
  {
    case QGraphicsItem::ItemScenePositionHasChanged:
    case QGraphicsItem::ItemVisibleHasChanged:
    case QGraphicsItem::ItemSceneHasChanged:
      for(auto cbl : cables)
      {
        cbl->check();
      }
      break;
    default:
      break;
  }

  return QGraphicsItem::itemChange(change, value);
}
class DefaultHeaderDelegate
    : public QObject
    , public Process::GraphicsShapeItem
{
  public:
    void onCreateCable(PortItem* p1, PortItem* p2)
    {
      auto& ctx = presenter.context().context;
      auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();
      CommandDispatcher<> disp{ctx.commandStack};
      Process::CableData cd;
      cd.type = Process::CableType::ImmediateStrict;

      if(p1->port().outlet)
      {
        cd.source = p1->port();
        cd.sink = p2->port();
      }
      else
      {
        cd.source = p2->port();
        cd.sink = p1->port();
      }

      disp.submitCommand<Dataflow::CreateCable>(
            plug,
            getStrongId(plug.cables),
            cd);
    }

    DefaultHeaderDelegate(Process::LayerPresenter& p)
      : presenter{p}
    {
      con(presenter.model(), &Process::ProcessModel::prettyNameChanged,
          this, &DefaultHeaderDelegate::updateName);
      updateName();
      m_textcache.setFont(ScenarioStyle::instance().Medium7Pt);
      m_textcache.setCacheEnabled(true);

      int x = 4;
      for(auto& port : p.model().inlets())
      {
        auto item = new PortItem{*port, this};
        item->setPos(x, 16);
        connect(item, &PortItem::showPanel,
                this, [&,&pt=*port,item] {
          auto panel = new PortPanel{p.context().context, pt, nullptr};
          scene()->addItem(panel);
          panel->setPos(item->mapToScene(item->pos()));
        });
        connect(item, &PortItem::createCable,
                this, &DefaultHeaderDelegate::onCreateCable);
        x += 10;
      }
      x = 4;
      for(auto& port : p.model().outlets())
      {
        auto item = new PortItem{*port, this};
        item->setPos(x, 25);
        connect(item, &PortItem::showPanel,
                this, [&,&pt=*port,item] {
          auto panel = new PortPanel{p.context().context, pt, nullptr};
          scene()->addItem(panel);
          panel->setPos(item->mapToScene(item->pos()));
        });
        x += 10;
      }
    }

    void updateName()
    {
      m_textcache.setText(presenter.model().prettyName());
      m_textcache.beginLayout();

      QTextLine line = m_textcache.createLine();
      line.setPosition(QPointF{0., 0.});

      m_textcache.endLayout();

      update();
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
      painter->setPen(ScenarioStyle::instance().IntervalHeaderSeparator);
      m_textcache.draw(painter, QPointF{4., 0.});

      painter->setPen(ScenarioStyle::instance().TimenodePen);
      painter->drawLine(QPointF{0, 19}, QPointF{boundingRect().width(), 19});
    }

  private:
    Process::LayerPresenter& presenter;
    QTextLayout m_textcache;
};

TemporalIntervalPresenter::TemporalIntervalPresenter(
    const IntervalModel& interval,
    const Process::ProcessPresenterContext& ctx,
    bool handles,
    QGraphicsItem* parentobject,
    QObject* parent)
  : IntervalPresenter{interval,
                      new TemporalIntervalView{*this, parentobject},
                      new TemporalIntervalHeader{*this}, ctx, parent}
  , m_handles{handles}
{
  TemporalIntervalView& v = *view();
  auto head = header();
  con(interval.selection, &Selectable::changed, this,
      [=] (bool b) {
    view()->setSelected(b);
    header()->enableOverlay(b);
  });

  con(v, &TemporalIntervalView::intervalHoverEnter, this,
      &TemporalIntervalPresenter::intervalHoverEnter);

  con(v, &TemporalIntervalView::intervalHoverLeave, this,
      &TemporalIntervalPresenter::intervalHoverLeave);

  con(v, &IntervalView::requestOverlayMenu,
      this, &TemporalIntervalPresenter::on_requestOverlayMenu);

  con(interval, &IntervalModel::executionStateChanged, &v,
      &TemporalIntervalView::setExecutionState);

  const auto& metadata = m_model.metadata();
  con(metadata, &score::ModelMetadata::LabelChanged, &v,
      &TemporalIntervalView::setLabel);

  con(metadata, &score::ModelMetadata::ColorChanged, &v,
      [&](score::ColorRef c) {
    v.setLabelColor(c);
    v.update();;
  });

  con(metadata, &score::ModelMetadata::NameChanged, this,
      [&](const QString& name) { m_header->setText(name); });

  v.setLabel(metadata.getLabel());
  v.setLabelColor(metadata.getColor());
  m_header->setText(metadata.getName());
  v.setExecutionState(m_model.executionState());

  con(m_model.selection, &Selectable::changed, this,
      [&,head] (bool b){
    v.setFocused(b);
    head->setFocused(b);
  });
  con(m_model, &IntervalModel::focusChanged, this,
      [&,head] (bool b){
    v.setFocused(b);
    head->setFocused(b);
  });

  // Drop
  con(v, &TemporalIntervalView::dropReceived, this,
      [=](const QPointF& pos, const QMimeData* mime) {
    m_context.app.interfaces<Scenario::IntervalDropHandlerList>()
        .drop(m_model, mime);
  });

  // Time
  con(interval.duration, &IntervalDurations::defaultDurationChanged, this,
      [&](const TimeVal& val) {
    on_defaultDurationChanged(val);
    updateChildren();
  });

  // Header set-up

  connect(
        head, &TemporalIntervalHeader::intervalHoverEnter, this,
        &TemporalIntervalPresenter::intervalHoverEnter);
  connect(
        head, &TemporalIntervalHeader::intervalHoverLeave, this,
        &TemporalIntervalPresenter::intervalHoverLeave);

  connect(
        head, &TemporalIntervalHeader::dropReceived, this,
        [=](const QPointF& pos, const QMimeData* mime) {
    m_context.app.interfaces<Scenario::IntervalDropHandlerList>()
        .drop(m_model, mime);
  });

  // Go to full-view on double click
  connect(head, &TemporalIntervalHeader::doubleClicked, this, [this]() {
    using namespace score::IDocument;
    ScenarioDocumentPresenter& base
        = get<ScenarioDocumentPresenter>(*documentFromObject(m_model));

    base.setDisplayedInterval(const_cast<IntervalModel&>(m_model));
  });

  // Slots & racks
  con(m_model, &IntervalModel::smallViewVisibleChanged, this,
      &TemporalIntervalPresenter::on_rackVisibleChanged);


  con(m_model, &IntervalModel::rackChanged,
      this, [=] (Slot::RackView t) {
    if(t == Slot::SmallView)
    {
      on_rackChanged();
    }
  });
  con(m_model, &IntervalModel::slotAdded,
      this, [=] (const SlotId& s) {
    if(s.smallView()) {
      createSlot(s.index, m_model.smallView()[s.index]);
    }
  });

  con(m_model, &IntervalModel::slotRemoved,
      this, [=] (const SlotId& s) {
    if(s.smallView())
      on_slotRemoved(s.index);
  });

  con(m_model, &IntervalModel::slotResized,
      this, [this] (const SlotId& s) {
    if(s.smallView())
      this->updatePositions();
  });

  con(m_model, &IntervalModel::layerAdded,
      this, [=] (SlotId s, Id<Process::ProcessModel> proc) {
    if(s.smallView())
      createLayer(s.index, m_model.processes.at(proc));
  });
  con(m_model, &IntervalModel::layerRemoved,
      this, [=] (SlotId s, Id<Process::ProcessModel> proc) {
    if(s.smallView())
      removeLayer(m_model.processes.at(proc));
  });
  con(m_model, &IntervalModel::frontLayerChanged,
      this, [=] (int pos, OptionalId<Process::ProcessModel> proc) {

    if(proc)
      on_layerModelPutToFront(pos, m_model.processes.at(*proc));
    // TODO else
  });

  m_model.processes.added.connect<TemporalIntervalPresenter, &TemporalIntervalPresenter::on_processesChanged>(this);
  m_model.processes.removed.connect<TemporalIntervalPresenter, &TemporalIntervalPresenter::on_processesChanged>(this);

  on_defaultDurationChanged(m_model.duration.defaultDuration());
  on_rackVisibleChanged(m_model.smallViewVisible());
}

TemporalIntervalPresenter::~TemporalIntervalPresenter()
{
  auto view = Scenario::view(this);
  // TODO deleteGraphicsObject
  if (view)
  {
    auto sc = view->scene();

    if (sc && sc->items().contains(view))
    {
      sc->removeItem(view);
    }

    view->deleteLater();
  }
}

void TemporalIntervalPresenter::on_requestOverlayMenu(QPointF)
{
  auto& fact = m_context.app.interfaces<Process::ProcessFactoryList>();
  auto dialog = new AddProcessDialog{fact, QApplication::activeWindow()};

  connect(
        dialog, &AddProcessDialog::okPressed, this,
        [&] (const auto& key) {
    auto cmd
        = new Scenario::Command::AddProcessToInterval(this->model(), key);

    CommandDispatcher<> d{m_context.commandStack};
    emit d.submitCommand(cmd);
  });

  dialog->launchWindow();
  dialog->deleteLater();
}

double TemporalIntervalPresenter::rackHeight() const
{
  qreal height = m_model.smallView().size() * (SlotHandle::handleHeight() + SlotHeader::headerHeight());
  for(const auto& slot : m_model.smallView())
  {
    height += slot.height;
  }
  return height;
}

void TemporalIntervalPresenter::updateHeight()
{
  if (m_model.smallViewVisible())
  {
    m_view->setHeight(rackHeight() + IntervalHeader::headerHeight());
  }
  else if (!m_model.smallViewVisible() && !m_model.processes.empty())
  {
    m_view->setHeight(IntervalHeader::headerHeight());
  }
  else
  {
    m_view->setHeight(8);
  }

  updateChildren();
  emit heightChanged();

}

void TemporalIntervalPresenter::on_rackVisibleChanged(bool b)
{
  if(b)
  {
    if(!m_model.processes.empty())
    {
      m_header->setState(IntervalHeader::State::RackShown);
    }
    else
    {
      m_header->setState(IntervalHeader::State::Hidden);
    }
  }
  else if(!m_model.processes.empty())
  {
    m_header->setState(IntervalHeader::State::RackHidden);
  }
  else
  {
    m_header->setState(IntervalHeader::State::Hidden);
  }

  on_rackChanged();
}

void TemporalIntervalPresenter::createSlot(int pos, const Slot& slt)
{
  if(m_model.smallViewVisible())
  {
    SlotPresenter p;
    p.header = new SlotHeader{*this, pos, m_view};
    if(m_handles)
      p.handle = new SlotHandle{*this, pos, m_view};
    // p.view = new SlotView{};
    m_slots.insert(m_slots.begin() + pos, std::move(p));

    for(const auto& process : slt.processes)
    {
      createLayer(pos, m_model.processes.at(process));
    }

    updatePositions();
  }
}

void TemporalIntervalPresenter::createLayer(int slot, const Process::ProcessModel& proc)
{
  if(m_model.smallViewVisible())
  {
    const auto& procKey = proc.concreteKey();

    auto factory = m_context.processList.findDefaultFactory(procKey);
    auto proc_view = factory->makeLayerView(proc, m_view);
    auto proc_pres = factory->makeLayerPresenter(proc, proc_view, m_context, this);
    proc_pres->on_zoomRatioChanged(m_zoomRatio);
    m_slots.at(slot).processes.push_back(LayerData{
                                           &proc, proc_pres, proc_view
                                         });

    auto con_id = con(
                    proc, &Process::ProcessModel::durationChanged, this,
                    [&] (const TimeVal&) {
      int i = 0;
      for(const SlotPresenter& slot : m_slots)
      {
        auto it = ossia::find_if(slot.processes,
                                 [&] (const LayerData& elt) {
          return elt.model->id() == proc.id();
        });

        if (it != slot.processes.end())
          updateProcessShape(i, *it);
        i++;
      }
    });

    con(proc, &IdentifiedObjectAbstract::identified_object_destroying, this,
        [=] { QObject::disconnect(con_id); });

    auto frontLayer = m_model.smallView().at(slot).frontProcess;
    if (frontLayer && (*frontLayer == proc.id()))
    {
      on_layerModelPutToFront(slot, proc);
    }
    else
    {
      on_layerModelPutToBack(slot, proc);
    }

    updatePositions();
  }
}

void TemporalIntervalPresenter::updateProcessShape(int slot, const LayerData& data)
{
  if(m_model.smallViewVisible())
  {
    data.presenter->setHeight(m_model.smallView().at(slot).height);

    auto width = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
    data.presenter->setWidth(width);
    data.presenter->parentGeometryChanged();
    data.view->update();
  }
}

void TemporalIntervalPresenter::removeLayer(const Process::ProcessModel& proc)
{
  if(m_model.smallViewVisible())
  {
    for(SlotPresenter& slot : m_slots)
    {
      boost::range::remove_erase_if(slot.processes, [&] (const LayerData& elt) {
        bool to_delete = elt.model->id() == proc.id();

        if (to_delete)
        {
          // No need to delete the view, the process presenters already do it.
          QPointer<Process::LayerView> view_p{elt.view};
          delete elt.presenter;
          if (view_p)
            deleteGraphicsItem(elt.view);
        }

        return to_delete;
      });
    }
  }
}

void TemporalIntervalPresenter::on_slotRemoved(int pos)
{
  if(m_model.smallViewVisible())
  {
    SlotPresenter& slot = m_slots.at(pos);
    for(LayerData& elt : slot.processes)
    {
      QPointer<Process::LayerView> view_p{elt.view};
      delete elt.presenter;
      if (view_p)
        deleteGraphicsItem(elt.view);
    }

    deleteGraphicsItem(slot.header);
    deleteGraphicsItem(slot.handle);
    //deleteGraphicsItem(slot.view);

    m_slots.erase(m_slots.begin() + pos);

    updatePositions();
  }
}

void TemporalIntervalPresenter::updateProcessesShape()
{
  for(int i = 0; i < (int)m_slots.size(); i++)
  {
    for(const LayerData& proc : m_slots[i].processes)
    {
      updateProcessShape(i, proc);
    }
  }
  updateScaling();
}

void TemporalIntervalPresenter::updatePositions()
{
  using namespace std;
  // Vertical shape
  m_view->setHeight(rackHeight() + IntervalHeader::headerHeight());

  // Set the slots position graphically in order.
  qreal currentSlotY = IntervalHeader::headerHeight();

  for(int i = 0; i < (int)m_slots.size(); i++)
  {
    const SlotPresenter& slot = m_slots[i];
    const Slot& model = m_model.smallView()[i];

    if(slot.header)
    {
      slot.header->setPos(QPointF{0, currentSlotY});
      slot.header->setSlotIndex(i);
    }
    currentSlotY += SlotHeader::headerHeight();

    for(const LayerData& proc : slot.processes)
    {
      proc.view->setPos(QPointF{0, currentSlotY});
      proc.view->update();
    }
    currentSlotY += model.height;

    if(slot.handle)
    {
      slot.handle->setPos(QPointF{1, currentSlotY});
      slot.handle->setSlotIndex(i);
    }
    currentSlotY += SlotHandle::handleHeight();
  }

  // Horizontal shape
  on_defaultDurationChanged(m_model.duration.defaultDuration());

  updateProcessesShape();
}
void TemporalIntervalPresenter::on_layerModelPutToFront(int slot, const Process::ProcessModel& proc)
{
  if(m_model.smallViewVisible())
  {
    // Put the selected one at z+1 and the others at -z; set "disabled" graphics
    // mode.
    // OPTIMIZEME by saving the previous to front and just switching...
    auto& slt = m_slots.at(slot);
    deleteGraphicsItem(slt.headerDelegate);
    slt.headerDelegate = nullptr;
    for (const LayerData& elt : slt.processes)
    {
      if (elt.model->id() == proc.id())
      {
        elt.presenter->putToFront();
        // slt.headerDelegate = elt.presenter->makeSlotHeaderDelegate();
        if(!slt.headerDelegate)
        {
          slt.headerDelegate = new DefaultHeaderDelegate{*elt.presenter};
        }
        slt.headerDelegate->setParentItem(slt.header);
        slt.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsToShape);
        slt.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsChildrenToShape);
        slt.headerDelegate->setPos(30, 0);
      }
      else
      {
        elt.presenter->putBehind();
      }
    }
  }
}

void TemporalIntervalPresenter::on_layerModelPutToBack(int slot, const Process::ProcessModel& proc)
{
  if(m_model.smallViewVisible())
  {
    for (const LayerData& elt : m_slots.at(slot).processes)
    {
      if (elt.model->id() == proc.id())
      {
        elt.presenter->putBehind();
        return;
      }
    }
  }
}

void TemporalIntervalPresenter::on_rackChanged()
{
  // Remove existing
  for(auto& slot : m_slots)
  {
    for(LayerData& elt : slot.processes)
    {
      QPointer<Process::LayerView> view_p{elt.view};
      delete elt.presenter;
      if (view_p)
        deleteGraphicsItem(elt.view);
    }

    deleteGraphicsItem(slot.header);
    deleteGraphicsItem(slot.handle);
  }

  m_slots.clear();

  // Recreate
  if(m_model.smallViewVisible())
  {
    m_slots.reserve(m_model.smallView().size());

    int i = 0;
    for(const auto& slt : m_model.smallView())
    {
      createSlot(i, slt);
      i++;
    }
  }

  // Update view
  updatePositions();
}

void TemporalIntervalPresenter::updateScaling()
{
  on_defaultDurationChanged(model().duration.defaultDuration());
  IntervalPresenter::updateScaling();
  updateHeight();
}

void TemporalIntervalPresenter::on_zoomRatioChanged(ZoomRatio val)
{
  IntervalPresenter::on_zoomRatioChanged(val);

  for(const SlotPresenter& slot : m_slots)
  {
    for(const LayerData& proc : slot.processes)
    {
      proc.presenter->on_zoomRatioChanged(val);
    }
  }

  updateProcessesShape();
}

void TemporalIntervalPresenter::changeRackState()
{
  ((IntervalModel&)m_model).setSmallViewVisible(!m_model.smallViewVisible() && !m_model.smallView().empty());
}

void TemporalIntervalPresenter::selectedSlot(int i) const
{
  score::SelectionDispatcher disp{m_context.selectionStack};
  SCORE_ASSERT(size_t(i) < m_slots.size());
  auto& slot = m_slots[i];
  if(slot.processes.empty())
  {
    disp.setAndCommit({&m_model});
  }
  else
  {
    auto proc = m_model.getSmallViewSlot(i).frontProcess;
    if(proc)
    {
      disp.setAndCommit({&m_model.processes.at(*proc)});
    }
  }
}

TemporalIntervalView*TemporalIntervalPresenter::view() const { return static_cast<TemporalIntervalView*>(this->m_view); }

TemporalIntervalHeader*TemporalIntervalPresenter::header() const
{ { return static_cast<TemporalIntervalHeader*>(this->m_header); }
}

void TemporalIntervalPresenter::requestSlotMenu(int slot, QPoint pos, QPointF sp) const
{
  if(const auto& proc = m_model.getSmallViewSlot(slot).frontProcess)
  {
    const SlotPresenter& slt = m_slots.at(slot);
    for(auto& p : slt.processes)
    {
      if(p.model->id() == proc)
      {
        auto menu = new QMenu;
        auto& reg = score::GUIAppContext()
                    .guiApplicationPlugin<ScenarioApplicationPlugin>()
                    .layerContextMenuRegistrar();
        ScenarioContextMenuManager::createLayerContextMenu(
              *menu, pos, sp, reg, *p.presenter);
        menu->exec(pos);
        menu->close();
        menu->deleteLater();
        break;
      }
    }
  }
}

void TemporalIntervalPresenter::on_defaultDurationChanged(const TimeVal& val)
{
  const auto w = val.toPixels(m_zoomRatio);
  m_view->setDefaultWidth(w);
  m_view->updateLabelPos();
  m_view->updateCounterPos();
  ((TemporalIntervalView*)m_view)->updateOverlayPos();
  m_header->setWidth(w);
  ((TemporalIntervalHeader*)m_header)->updateButtons();
  updateBraces();

  for(const SlotPresenter& slot : m_slots)
  {
    slot.header->setWidth(w);
    if(slot.handle)
      slot.handle->setWidth(w);
    if(slot.headerDelegate)
      slot.headerDelegate->setSize(QSizeF{w - SlotHeader::handleWidth() - SlotHeader::menuWidth(), SlotHeader::headerHeight()});
    for(const LayerData& proc : slot.processes)
    {
      proc.presenter->setWidth(w);
    }
  }
}

int TemporalIntervalPresenter::indexOfSlot(const Process::LayerPresenter& proc)
{
  if(m_model.smallViewVisible())
  {
    for(int i = 0; i < (int)m_slots.size(); ++i)
    {
      const auto& p = m_slots[i].processes;
      for(int j = 0; j < (int)p.size(); j++)
      {
        if(p[j].presenter == &proc)
          return i;
      }
    }
  }

  SCORE_ABORT;
}

void TemporalIntervalPresenter::on_processesChanged(const Process::ProcessModel&)
{
  if(m_model.smallViewVisible())
  {
    m_header->setState(IntervalHeader::State::RackShown);
  }
  else if(!m_model.processes.empty())
  {
    m_header->setState(IntervalHeader::State::RackHidden);
  }
  else
  {
    m_header->setState(IntervalHeader::State::Hidden);
  }
}

}
