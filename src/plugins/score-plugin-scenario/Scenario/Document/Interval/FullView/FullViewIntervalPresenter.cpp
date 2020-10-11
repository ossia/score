// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "FullViewIntervalPresenter.hpp"

#include "AddressBarItem.hpp"
#include "FullViewIntervalHeader.hpp"

#include <Automation/AutomationColors.hpp>
#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/HeaderDelegate.hpp>
#include <Process/LayerView.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Style/Pixmaps.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Application/Menus/ScenarioContextMenuManager.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Interval/Rack/SwapSlots.hpp>
#include <Scenario/Document/Interval/DefaultHeaderDelegate.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalView.hpp>
#include <Scenario/Document/Interval/FullView/NodalIntervalView.hpp>
#include <Scenario/Document/Interval/FullView/TimeSignatureItem.hpp>
#include <Scenario/Document/Interval/FullView/Timebar.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/LayerData.hpp>
#include <Scenario/Document/Interval/SlotHeader.hpp>
#include <Scenario/Document/ScenarioDocument/MusicalGrid.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/closest_element.hpp>
#include <ossia/detail/flicks.hpp>

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMenu>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::FullViewIntervalPresenter)
W_OBJECT_IMPL(Scenario::TimeSignatureHandle)
W_OBJECT_IMPL(Scenario::LineTextItem)

namespace Scenario
{
static int timeSignatureHeight = 20;
static double timeSignatureBarY = -45.;

static SlotDragOverlay* full_slot_drag_overlay{};

Timebars::Timebars(FullViewIntervalPresenter& self) : timebar{self, self.view()}
{
  timebar.setPos(0, -47);
}

void FullViewIntervalPresenter::startSlotDrag(int curslot, QPointF pos) const
{
  // Create an overlay object
  full_slot_drag_overlay = new SlotDragOverlay{*this, Slot::FullView};
  connect(
      full_slot_drag_overlay,
      &SlotDragOverlay::dropBefore,
      this,
      [=](int slot) {
        CommandDispatcher<>{this->m_context.commandStack}.submit<Command::ChangeSlotPosition>(
            this->m_model, Slot::RackView::FullView, curslot, slot);
      },
      Qt::QueuedConnection); // needed because else SlotHeader is removed and
                             // stopSlotDrag can't be called

  full_slot_drag_overlay->setParentItem(view());
  full_slot_drag_overlay->onDrag(pos);
}

void FullViewIntervalPresenter::stopSlotDrag() const
{
  delete full_slot_drag_overlay;
  full_slot_drag_overlay = nullptr;
}

FullViewIntervalPresenter::FullViewIntervalPresenter(
    const IntervalModel& interval,
    const Process::Context& ctx,
    QGraphicsItem* parentobject,
    QObject* parent)
    : IntervalPresenter{interval, new FullViewIntervalView{*this, parentobject}, new FullViewIntervalHeader{ctx, parentobject}, ctx, parent}
    , m_timebars{new Timebars{*this}}
    , m_grid{new MusicalGrid{*m_timebars}}
    , m_settings{ctx.app.settings<Scenario::Settings::Model>()}
{
  m_view->setPos(0, 0);
  m_header->setPos(0, -IntervalHeader::headerHeight());

  m_timebars->lightBars.setParentItem(m_view);
  m_timebars->lighterBars.setParentItem(m_view);

  // Address bar
  auto& addressBar = static_cast<FullViewIntervalHeader*>(m_header)->bar();
  addressBar.setTargetObject(score::IDocument::unsafe_path(interval));
  con(addressBar,
      &AddressBarItem::intervalSelected,
      this,
      &FullViewIntervalPresenter::intervalSelected);

  con(interval.selection,
      &Selectable::changed,
      (FullViewIntervalView*)m_view,
      &FullViewIntervalView::setSelected);

  // Time
  con(interval.duration,
      &IntervalDurations::defaultDurationChanged,
      this,
      [&](const TimeVal& val) { on_defaultDurationChanged(val); });
  con(interval.duration, &IntervalDurations::guiDurationChanged, this, [&](const TimeVal& val) {
    on_guiDurationChanged(val);
    updateChildren();
  });

  auto& settings = m_context.app.settings<Scenario::Settings::Model>();
  con(m_model, &IntervalModel::timeSignaturesChanged, this, [=] { updateTimeBars(); });
  con(m_model, &IntervalModel::hasTimeSignatureChanged, this, [=] { updateTimeBars(); });
  ::bind(settings, Settings::Model::p_MeasureBars{}, this, [=](bool show) {
    this->m_timebars->lightBars.setVisible(show);
    this->m_timebars->lighterBars.setVisible(show);
  });

  // Slots
  con(m_model, &IntervalModel::rackChanged, this, [=](Slot::RackView t) {
    if (t == Slot::FullView)
      on_rackChanged();
  });

  con(m_model, &IntervalModel::slotAdded, this, [=](const SlotId& s) {
    if (s.fullView())
      on_rackChanged();
  });

  con(m_model, &IntervalModel::slotRemoved, this, [=](const SlotId& s) {
    if (s.fullView())
      on_rackChanged();
  });

  con(m_model, &IntervalModel::slotsSwapped, this, [=](int i, int j, Slot::RackView v) {
    if (v == Slot::FullView)
      on_rackChanged();
  });

  con(m_model, &IntervalModel::slotResized, this, [this](const SlotId& s) {
    if (s.fullView())
      this->updatePositions();
  });

  // Execution
  con(
      interval,
      &IntervalModel::executionStarted,
      this,
      [=] {
        m_view->setExecuting(true);
        m_view->updatePaths();
        m_view->update();
      },
      Qt::QueuedConnection);
  con(
      interval,
      &IntervalModel::executionStopped,
      this,
      [=] { m_view->setExecuting(false); },
      Qt::QueuedConnection);
  con(
      interval,
      &IntervalModel::executionFinished,
      this,
      [=] {
        m_view->setExecuting(false);
        m_view->setPlayWidth(0.);
        m_view->updatePaths();
        m_view->update();
      },
      Qt::QueuedConnection);

  // Drops

  con(*this->view(),
      &IntervalView::dropReceived,
      this,
      [=](const QPointF& pos, const QMimeData& mime) {
        m_context.app.interfaces<Scenario::IntervalDropHandlerList>().drop(
            m_context, m_model, {}, mime);
      });

  // Initial state
  on_rackChanged();
}

FullViewIntervalPresenter::~FullViewIntervalPresenter()
{
  delete m_grid;
  auto view = Scenario::view(this);
  QGraphicsScene* sc = view->scene();

  for (auto& slt : m_slots)
    slt.cleanup(sc);

  if (sc)
    sc->removeItem(view);

  delete m_timebars;
  delete view;
}

void FullViewIntervalPresenter::createSlot(int slot_i, const FullSlot& s)
{
  if(!s.nodal)
  {
    // Create the slot
    LayerSlotPresenter p;
    p.header = new SlotHeader{*this, slot_i, m_view};
    p.footer = new AmovibleSlotFooter{*this, slot_i, m_view};
    auto it = m_slots.insert(m_slots.begin() + slot_i, std::move(p));
    auto& slt = *it->getLayerSlot();
    setupSlot(slt,  m_model.processes.at(s.process), slot_i);
  }
  else
  {
    NodalSlotPresenter p;
    p.header = new SlotHeader{*this, slot_i, m_view};
    auto nodal = new NodalIntervalView{NodalIntervalView::OnlyEffects, this->model(), this->context(), this->view()};
    nodal->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);
    p.view = nodal;
    p.footer = new AmovibleSlotFooter{*this, slot_i, m_view};
    auto it = m_slots.insert(m_slots.begin() + slot_i, std::move(p));

    setupSlot(*it->getNodalSlot(), slot_i);
  }
}

void FullViewIntervalPresenter::setupSlot(NodalSlotPresenter& slot, int slot_i)
{
  updateProcessShape(slot_i);
}

void FullViewIntervalPresenter::setupSlot(LayerSlotPresenter& slot, const Process::ProcessModel& proc, int slot_i)
{
  // Create a layer container
  auto& ld = slot.layers.emplace_back(&proc);

  // Create layers
  const auto factory = m_context.processList.findDefaultFactory(proc.concreteKey());

  const auto gui_width = m_model.duration.guiDuration().toPixels(m_zoomRatio);
  const auto def_width = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
  const auto slot_height = ld.model().getSlotHeight();
  ld.updateLoops(m_context, m_zoomRatio, gui_width, def_width, slot_height, m_view, this);

  {
    slot.headerDelegate = factory->makeHeaderDelegate(proc, m_context, slot.header);
    slot.headerDelegate->updateText();
    slot.headerDelegate->setParentItem(slot.header);
    // slot.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsToShape);
    // slot.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsChildrenToShape);
    slot.headerDelegate->setPos(30, 0);
  }
  {
    slot.footerDelegate = factory->makeFooterDelegate(proc, m_context);
    slot.footerDelegate->setParentItem(slot.footer);
    // slot.footerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsToShape);
    // slot.footerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsChildrenToShape);
    slot.footerDelegate->setPos(30, 0);
  }

  // Connect loop things
  LayerData::disconnect(proc, *this);

  con(proc, &Process::ProcessModel::loopsChanged, this, [this, slot_i] (bool b) {
    SCORE_ASSERT(slot_i < int(m_slots.size()));
    auto slt = this->m_slots[slot_i].getLayerSlot();

    SCORE_ASSERT(slt);
    SCORE_ASSERT(!slt->layers.empty());
    LayerData& ld = slt->layers.front();
    const auto gui_width = m_model.duration.guiDuration().toPixels(m_zoomRatio);
    const auto def_width = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
    const auto slot_height = ld.model().getSlotHeight();
    ld.updateLoops(m_context, m_zoomRatio, gui_width, def_width, slot_height, this->m_view, this);
  });

  con(proc, &Process::ProcessModel::startOffsetChanged, this, [this, slot_i] {
    SCORE_ASSERT(slot_i < int(m_slots.size()));
    auto slt = this->m_slots[slot_i].getLayerSlot();

    SCORE_ASSERT(slt);
    SCORE_ASSERT(!slt->layers.empty());
    auto& ld = slt->layers.front();

    ld.updateStartOffset(-ld.model().startOffset().toPixels(m_zoomRatio));
  });
  con(proc, &Process::ProcessModel::loopDurationChanged, this, [this, slot_i] {
    SCORE_ASSERT(slot_i < int(m_slots.size()));
    auto slt = this->m_slots[slot_i].getLayerSlot();

    SCORE_ASSERT(slt);
    SCORE_ASSERT(!slt->layers.empty());
    LayerData& ld = slt->layers.front();
    const auto gui_width = m_model.duration.guiDuration().toPixels(m_zoomRatio);
    const auto def_width = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
    const auto slot_height = ld.model().getSlotHeight();
    ld.updateLoops(m_context, m_zoomRatio, gui_width, def_width, slot_height, this->m_view, this);
  });

  updateProcessShape(slot_i);
}

void FullViewIntervalPresenter::requestSlotMenu(int slot, QPoint pos, QPointF sp) const
{
  auto slt = m_slots[slot].getLayerSlot();
  if(!slt)
    return;

  auto menu = new QMenu;
  auto& reg = score::GUIAppContext()
                  .guiApplicationPlugin<ScenarioApplicationPlugin>()
                  .layerContextMenuRegistrar();

  slt->layers.front().fillContextMenu(*menu, pos, sp, reg);
  menu->exec(pos);
  menu->close();
  menu->deleteLater();
}

void FullViewIntervalPresenter::updateProcessShape(const LayerData& ld, const LayerSlotPresenter& slot)
{
  const auto h = ld.model().getSlotHeight();
  ld.setHeight(h);
  ld.updateContainerHeights(h); // TODO merge with setHeight

  auto width = m_model.duration.guiDuration().toPixels(m_zoomRatio);
  auto dwidth = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
  ld.setWidth(width, dwidth);

  slot.header->setWidth(width);
  slot.footer->setWidth(width);

  slot.headerDelegate->setSize(QSizeF{
      std::max(0., width - SlotHeader::handleWidth() - SlotHeader::menuWidth()),
      SlotHeader::headerHeight()});
  slot.headerDelegate->setX(30);
  slot.footerDelegate->setSize(QSizeF{width, SlotFooter::footerHeight()});
  slot.footerDelegate->setX(30);

  ld.parentGeometryChanged();
}

void FullViewIntervalPresenter::updateProcessShape(LayerSlotPresenter& slot, int idx)
{
  if (!slot.layers.empty())
    updateProcessShape(slot.layers.front(), slot);
}

void FullViewIntervalPresenter::updateProcessShape(NodalSlotPresenter& slot, int idx)
{
  const auto h =  this->model().getSlotHeight({idx, Slot::FullView});
  auto width = m_model.duration.guiDuration().toPixels(m_zoomRatio);
  //auto dwidth = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
  slot.header->setWidth(width);
  slot.view->setRect({0, 0, width, h});
  slot.height = h;
  slot.footer->setWidth(width);
}

void FullViewIntervalPresenter::updateProcessShape(int idx)
{
  auto& slt = m_slots.at(idx);
  slt.visit([=] (auto& slot) { updateProcessShape(slot, idx); });
}

void FullViewIntervalPresenter::on_slotRemoved(int pos)
{
  SlotPresenter& slot = m_slots.at(pos);
  auto lay_slt = slot.getLayerSlot();
  if(lay_slt)
  {
    for (const LayerData& proc : lay_slt->layers)
    {
      proc.disconnect(proc.model(), *this);
    }
  }
  slot.cleanup(this->view()->scene());

  m_slots.erase(m_slots.begin() + pos);
}

void FullViewIntervalPresenter::updateProcessesShape()
{
  for (int i = 0; i < (int)m_slots.size(); i++)
  {
    updateProcessShape(i);
  }
}

void FullViewIntervalPresenter::updatePositions()
{
  using namespace std;
  // Vertical shape
  m_view->setHeight(rackHeight() + IntervalHeader::headerHeight());

  // Set the slots position graphically in order.
  qreal currentSlotY = 2.;

  for (int i = 0; i < (int)m_slots.size(); i++)
  {
    m_slots[i].visit(
          [&] (LayerSlotPresenter& slot) {

      if (!slot.layers.empty())
      {
        LayerData& ld = slot.layers.front();

        assert(slot.header);
        slot.header->setPos(QPointF{0, currentSlotY});
        slot.header->setSlotIndex(i);
        currentSlotY += slot.headerHeight();

        ld.updateYPositions(currentSlotY);
        ld.update();

        currentSlotY += ld.model().getSlotHeight();
        assert(slot.footer);
        slot.footer->setPos(QPointF{0, currentSlotY});
        slot.footer->setSlotIndex(i);
        currentSlotY += slot.footerHeight();
      }
      else
      {
        currentSlotY += SlotHeader::headerHeight();
        currentSlotY += SlotFooter::footerHeight();
      }
    },
    [&] (NodalSlotPresenter& slot) {

      assert(slot.header);
      slot.header->setPos(QPointF{0, currentSlotY});
      slot.header->setSlotIndex(i);
      currentSlotY += SlotHeader::headerHeight();

      slot.view->setPos(QPointF{0, currentSlotY});
      updateProcessShape(slot, i);
      currentSlotY += slot.height;

      assert(slot.footer);
      slot.footer->setPos(QPointF{0, currentSlotY});
      slot.footer->setSlotIndex(i);
      currentSlotY += SlotFooter::footerHeight();
    }
    );
  }

  // Horizontal shape
  on_defaultDurationChanged(m_model.duration.defaultDuration());
  on_guiDurationChanged(m_model.duration.guiDuration());

  updateProcessesShape();
  updateHeight();
}

double FullViewIntervalPresenter::rackHeight() const
{
  qreal height = 0;
  for (const SlotPresenter& slt : m_slots)
  {
    if(auto slot = slt.getLayerSlot())
    {
      if (!slot->layers.empty())
        height += slot->layers.front().model().getSlotHeight();

      height += slot->headerHeight() + slot->footerHeight();
    }
    else if(auto slot = slt.getNodalSlot())
    {
      height += SlotHeader::headerHeight();
      height += slot->height;
      height += SlotFooter::footerHeight();
    }
  }
  return height;
}

void FullViewIntervalPresenter::on_rackChanged()
{
  // Remove existing
  if (!m_slots.empty())
  {
    for (int i = (int)m_slots.size(); i-- > 0;)
    {
      on_slotRemoved(i);
    }
  }

  // Recreate
  m_slots.reserve(m_model.fullView().size());

  int i = 0;
  for (const auto& slt : m_model.fullView())
  {
    createSlot(i, slt);
    i++;
  }

  // Update view
  updatePositions();
}

void FullViewIntervalPresenter::updateScaling()
{
  on_defaultDurationChanged(model().duration.defaultDuration());
  on_guiDurationChanged(model().duration.guiDuration());
  IntervalPresenter::updateScaling();
  updateHeight();
}

void FullViewIntervalPresenter::selectedSlot(int i) const
{
  score::SelectionDispatcher disp{m_context.selectionStack};
  SCORE_ASSERT(size_t(i) < m_slots.size());
  auto slot = m_slots[i].getLayerSlot();
  if(slot)
  {
    if (!slot->layers.empty())
    {
      if(auto pres = slot->layers.front().mainPresenter())
      {
        m_context.focusDispatcher.focus(pres);
        disp.setAndCommit({&slot->layers.front().model()});
      }
      else
      {
        SCORE_SOFT_ASSERT(!"No main presenter");
      }
    }
    else
    {
      SCORE_SOFT_ASSERT(!"No layer!");
    }
  }
}

void FullViewIntervalPresenter::on_defaultDurationChanged(const TimeVal& val)
{
  const auto w = val.toPixels(m_zoomRatio);
  m_view->setDefaultWidth(w);
  m_view->updateCounterPos();
  ((FullViewIntervalView*)m_view)->updateOverlayPos();

  m_header->update();
  m_view->update();
}

void FullViewIntervalPresenter::on_zoomRatioChanged(ZoomRatio ratio)
{
  IntervalPresenter::on_zoomRatioChanged(ratio);

  updateTimeBars();

  auto gui_width = m_model.duration.guiDuration().toPixels(ratio);
  auto def_width = m_model.duration.defaultDuration().toPixels(ratio);

  m_timebars->timebar.setWidth(gui_width);

  for (auto& slot : m_slots)
  {
    if(auto lay_slot = slot.getLayerSlot())
    {
      if (lay_slot->headerDelegate)
        lay_slot->headerDelegate->on_zoomRatioChanged(ratio);
      for (auto& ld : lay_slot->layers)
      {
        const auto slot_height = ld.model().getSlotHeight();
        ld.on_zoomRatioChanged(m_context, ratio, gui_width, def_width, slot_height, m_view, this);
      }
    }
  }

  updateProcessesShape();
}

Process::MagneticInfo
FullViewIntervalPresenter::magneticPosition(const QObject* o, const TimeVal t) const noexcept
{
  // TODO instead call a virtual function on the process that return the date
  // of the closest thing If it's less close than the grid... return
  TimeVal scenarioT = t;
  TimeVal closestTimeSyncT = TimeVal::fromMsecs(std::numeric_limits<int32_t>::max());
  bool snapToScenario{};
  if (auto given_ts = qobject_cast<const Scenario::TimeSyncModel*>(o))
  {
    if (auto scenario = qobject_cast<Scenario::ProcessModel*>(given_ts->parent()))
    {
      for (auto& ts : scenario->timeSyncs)
      {
        if (given_ts == &ts)
          continue;

        if (std::abs(ts.date().impl - t.impl) < std::abs(closestTimeSyncT.impl - t.impl))
        {
          closestTimeSyncT = ts.date();
        }
      }
      double delta = std::abs((closestTimeSyncT - t).toPixels(m_zoomRatio));
      if (delta < 10)
      {
        scenarioT = closestTimeSyncT;
        snapToScenario = true;
      }
    }
  }

  if (!m_settings.getMagneticMeasures() || !m_settings.getMeasureBars())
    return {scenarioT, snapToScenario};

  // t is the time in the context of obj
  // we have to find its closest parent interval with a time signature
  // definition and compute its time delta
  const IntervalModel* cur_model = nullptr;
  do {
   cur_model = qobject_cast<const IntervalModel*>(o);
   if(cur_model)
     break;
   else
     o = o->parent();
  } while(!cur_model && o);
  auto [model, timeDelta] = closestParentWithMusicalMetrics(&m_model);

  if (!o || !model)
    return {scenarioT, snapToScenario};

  // Find leftmost signature
  const TimeVal msecs = t + timeDelta;
  const auto& sig = model->timeSignatureMap();
  if (sig.empty())
    return {scenarioT, snapToScenario};

  auto leftmost_sig = sig.lower_bound(msecs);
  if (leftmost_sig != sig.begin())
    leftmost_sig--;

  // Snap to grid
  if (m_timebars->magneticTimings.empty())
    return {scenarioT, snapToScenario};

  const TimeVal& closestBar = closest_element(m_timebars->magneticTimings, msecs);
  if (!snapToScenario)
  {
    return {closestBar - timeDelta, snapToScenario};
  }
  else if (std::abs(closestBar.impl - t.impl) < std::abs(scenarioT.impl - t.impl))
  {
    return {closestBar - timeDelta, false};
  }
  else
  {
    return {scenarioT, snapToScenario};
  }
}

double FullViewIntervalPresenter::on_playPercentageChanged(double t)
{
  return IntervalPresenter::on_playPercentageChanged(t);
}

MusicalGrid& FullViewIntervalPresenter::grid() const noexcept
{
  return *m_grid;
}

void FullViewIntervalPresenter::on_visibleRectChanged(QRectF r)
{
  if (r != m_sceneRect)
  {
    m_sceneRect = r;
    updateTimeBars();
  }
}

void FullViewIntervalPresenter::setSnapLine(TimeVal t, bool enabled)
{

}

void FullViewIntervalPresenter::updateTimeBars()
{
  if (m_zoomRatio <= 0.)
    return;

  auto [model, timeDelta] = closestParentWithMusicalMetrics(&m_model);

  this->m_timebars->timebar.setModel(model, timeDelta);

  if (!model || !this->m_settings.getMeasureBars())
  {
    if (this->m_timebars->timebar.isEnabled())
    {
      this->m_timebars->lightBars.setVisible(false);
      this->m_timebars->lighterBars.setVisible(false);
      this->m_timebars->timebar.setEnabled(false);
      this->m_timebars->timebar.setVisible(false);
    }
    return;
  }
  else
  {
    if (!this->m_timebars->timebar.isEnabled())
    {
      this->m_timebars->lightBars.setVisible(true);
      this->m_timebars->lighterBars.setVisible(true);
      this->m_timebars->timebar.setEnabled(true);
      this->m_timebars->timebar.setVisible(true);
    }
  }

  m_timebars->timebar.setZoomRatio(m_zoomRatio);

  auto scene_x0 = m_view->mapToScene(QPointF{}).x();

  QRectF sceneRect = m_sceneRect;
  sceneRect.adjust(-100, timeSignatureBarY, 100, timeSignatureBarY);

  // x0_time: time of the currently visible leftmost pixel
  TimeVal x0_time = TimeVal::fromPixels(sceneRect.x() - scene_x0, m_zoomRatio) + timeDelta;

  m_grid->setMeasures(model->timeSignatureMap());
  m_grid->compute(timeDelta, m_zoomRatio, sceneRect, x0_time);
}

void FullViewIntervalPresenter::on_modeChanged(IntervalModel::ViewMode m)
{
  // on_rackChanged();
  // updateTimeBars();
}

void FullViewIntervalPresenter::on_guiDurationChanged(const TimeVal& val)
{
  const auto gui_width = val.toPixels(m_zoomRatio);
  const auto def_width = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
  m_header->setWidth(gui_width);
  m_timebars->timebar.setWidth(gui_width);

  static_cast<FullViewIntervalView*>(m_view)->setGuiWidth(gui_width);

  for (SlotPresenter& slot : m_slots)
  {
    slot.visit([&] (auto& slot) { on_guiDurationChanged(slot, gui_width, def_width); });
  }
}

void FullViewIntervalPresenter::on_guiDurationChanged(LayerSlotPresenter& slot, double gui_width, double def_width)
{
  if (!slot.layers.empty())
  {
    auto& ld = slot.layers.front();
    const auto slot_height = ld.model().getSlotHeight();

    ld.updateLoops(m_context, m_zoomRatio, gui_width, def_width, slot_height, m_view, this);
  }
  slot.header->setWidth(gui_width);
  slot.footer->setWidth(gui_width);

}

void FullViewIntervalPresenter::on_guiDurationChanged(NodalSlotPresenter& slot, double gui_width, double def_width)
{
  slot.header->setWidth(gui_width);
  slot.footer->setWidth(gui_width);
}

void FullViewIntervalPresenter::updateHeight()
{
  m_view->setHeight(rackHeight() + IntervalHeader::headerHeight());

  updateChildren();
  heightChanged();
}
}
