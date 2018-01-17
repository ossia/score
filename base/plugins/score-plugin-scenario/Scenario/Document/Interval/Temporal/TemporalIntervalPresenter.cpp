// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/ProcessList.hpp>
#include <QGraphicsScene>
#include <QList>
#include <QApplication>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/CreateProcessInNewSlot.hpp>
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
#include <Scenario/Document/Interval/Temporal/DefaultHeaderDelegate.hpp>

namespace Scenario
{

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
    v.update();
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

    ScenarioDocumentPresenter* base
        = get<ScenarioDocumentPresenter>(*documentFromObject(m_model));

    if(base)
      base->setDisplayedInterval(const_cast<IntervalModel&>(m_model));
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
  auto dialog = new AddProcessDialog{fact, Process::ProcessFlags::SupportsTemporal, QApplication::activeWindow()};

  dialog->on_okPressed =
        [&] (const auto& key, QString dat) {
    using namespace Scenario::Command;

    if(fact.get(key)->flags() & Process::ProcessFlags::PutInNewSlot)
    {
      QuietMacroCommandDispatcher<AddProcessInNewSlot> d{m_context.commandStack};
      AddProcessInNewSlot::create(d, this->model(), key, dat);
      d.commit();
    }
    else
    {
      CommandDispatcher<> d{m_context.commandStack};
      d.submitCommand<AddProcessToInterval>(this->model(), key, dat);
    }
  };

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
    SlotPresenter& slt = m_slots.at(slot);
    deleteGraphicsItem(slt.headerDelegate);
    slt.headerDelegate = nullptr;
    for (const LayerData& elt : slt.processes)
    {
      if (elt.model->id() == proc.id())
      {
        elt.presenter->putToFront();
        slt.headerDelegate = new DefaultHeaderDelegate{*elt.presenter};
        slt.headerDelegate->setParentItem(slt.header);
        slt.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsToShape);
        slt.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsChildrenToShape);
        slt.headerDelegate->setPos(30, 0);

        setHeaderWidth(slt, m_model.duration.defaultDuration().toPixels(m_zoomRatio));
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
      m_context.focusDispatcher.focus(&m_slots[i].headerDelegate->presenter);
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

void TemporalIntervalPresenter::setHeaderWidth(const SlotPresenter& slot, double w)
{
  slot.header->setWidth(w);
  if(slot.handle)
    slot.handle->setWidth(w);

  if(slot.headerDelegate)
  {
    auto pw = slot.headerDelegate->minPortWidth();
    if(w - SlotHeader::handleWidth() - SlotHeader::menuWidth() >= pw) {
      slot.header->setMini(false);

      slot.headerDelegate->setSize(QSizeF{std::max(0., w - SlotHeader::handleWidth() - SlotHeader::menuWidth()), SlotHeader::headerHeight()});
      slot.headerDelegate->setX(30);
    }
    else {
      slot.header->setMini(true);

      slot.headerDelegate->setSize(QSizeF{w, SlotHeader::headerHeight()});
      slot.headerDelegate->setX(0);
    }
  }
  else
  {
    slot.header->setMini(false);
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
    setHeaderWidth(slot, w);

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
