// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "FullViewIntervalPresenter.hpp"

#include "AddressBarItem.hpp"
#include "FullViewIntervalHeader.hpp"

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/HeaderDelegate.hpp>
#include <Process/LayerView.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Application/Menus/ScenarioContextMenuManager.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Interval/Rack/SwapSlots.hpp>
#include <Scenario/Document/Interval/DefaultHeaderDelegate.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalView.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/LayerData.hpp>
#include <Scenario/Document/Interval/SlotHandle.hpp>
#include <Scenario/Document/Interval/SlotHeader.hpp>
#include <Scenario/Document/Interval/FullView/TimeSignatureItem.hpp>
#include <Scenario/Document/Interval/FullView/NodalIntervalView.hpp>
#include <Scenario/Document/Interval/FullView/Timebar.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Style/Pixmaps.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/tools/Bind.hpp>
#include <ossia/detail/algorithms.hpp>

#include <QGraphicsScene>
#include <QMenu>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::FullViewIntervalPresenter)
W_OBJECT_IMPL(Scenario::TimeSignatureHandle)
namespace Scenario
{
struct Timebars
{
  TimeSignatureItem timebar;

  std::array<LightTimebar, 200> lightBars;
  std::array<LighterTimebar, 600> lighterBars;
};

static SlotDragOverlay* full_slot_drag_overlay{};


void FullViewIntervalPresenter::startSlotDrag(int curslot, QPointF pos) const
{
  // Create an overlay object
  full_slot_drag_overlay = new SlotDragOverlay{*this, Slot::FullView};
  connect(
      full_slot_drag_overlay,
      &SlotDragOverlay::dropBefore,
      this,
      [=](int slot) {
        CommandDispatcher<>{this->m_context.commandStack}
            .submit<Command::ChangeSlotPosition>(
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
    : IntervalPresenter{interval,
                        new FullViewIntervalView{*this, parentobject},
                        new FullViewIntervalHeader{ctx, parentobject},
                        ctx,
                        parent}

    , m_timebars{new Timebars{{*this, m_view}, {}, {}}}
{
  m_header->setPos(0, -IntervalHeader::headerHeight());

  for(auto& bar : m_timebars->lightBars)
  {
    bar.setParentItem(m_view);
  }

  for(auto& bar : m_timebars->lighterBars)
  {
    bar.setParentItem(m_view);
  }

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

  // Address bar items
  {
    auto& pixmaps = Process::Pixmaps::instance();
    auto nodalButton = new score::QGraphicsPixmapToggle{pixmaps.nodal_on, pixmaps.nodal_off, &addressBar};
    auto timelineButton = new score::QGraphicsPixmapToggle{pixmaps.timeline_on, pixmaps.timeline_off, &addressBar};
    switch(interval.viewMode())
    {
      case IntervalModel::Temporal:
        timelineButton->toggle();
        break;
      case IntervalModel::Nodal:
        nodalButton->toggle();
        break;
    }

    connect(nodalButton, &score::QGraphicsPixmapToggle::toggled,
            this, [=] (bool state) {
      ((FullViewIntervalPresenter&)m_view->presenter()).requestModeChange(state);
      timelineButton->toggle();
    });
    connect(timelineButton, &score::QGraphicsPixmapToggle::toggled,
            this, [=] (bool state) {
      ((FullViewIntervalPresenter&)m_view->presenter()).requestModeChange(!state);
      nodalButton->toggle();
    });
    nodalButton->setPos(-50, -4);
    timelineButton->setPos(-30, -4);
  }


  // Time
  con(interval.duration,
      &IntervalDurations::defaultDurationChanged,
      this,
      [&](const TimeVal& val) { on_defaultDurationChanged(val); });
  con(interval.duration,
      &IntervalDurations::guiDurationChanged,
      this,
      [&](const TimeVal& val) {
        on_guiDurationChanged(val);
        updateChildren();
      });

  // Slots
  con(m_model, &IntervalModel::rackChanged, this, [=](Slot::RackView t) {
    if (t == Slot::FullView && !m_nodal)
      on_rackChanged();
  });

  con(m_model, &IntervalModel::slotAdded, this, [=](const SlotId& s) {
    if (s.fullView() && !m_nodal)
      on_rackChanged();
  });

  con(m_model, &IntervalModel::slotRemoved, this, [=](const SlotId& s) {
    if (s.fullView() && !m_nodal)
      on_rackChanged();
  });

  con(m_model,
      &IntervalModel::slotsSwapped,
      this,
      [=](int i, int j, Slot::RackView v) {
        if (v == Slot::FullView && !m_nodal)
          on_rackChanged();
      });

  con(m_model, &IntervalModel::slotResized, this, [this](const SlotId& s) {
    if (s.fullView() && !m_nodal)
      this->updatePositions();
  });

  // Execution
  con(interval,
      &IntervalModel::executionStarted,
      this,
      [=] {
        m_view->setExecuting(true);
        m_view->updatePaths();
        m_view->update();
      },
      Qt::QueuedConnection);
  con(interval,
      &IntervalModel::executionStopped,
      this,
      [=] {
         m_view->setExecuting(false);
      },
      Qt::QueuedConnection);
  con(interval,
      &IntervalModel::executionFinished,
      this,
      [=] {
        m_view->setExecuting(false);
        m_view->setPlayWidth(0.);
        if(m_nodal)
        {
          m_nodal->on_playPercentageChanged(0.);
        }
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
            m_context, m_model, mime);
      });


  // Initial state
  on_rackChanged();
}

FullViewIntervalPresenter::~FullViewIntervalPresenter()
{
  auto view = Scenario::view(this);
  QGraphicsScene* sc = view->scene();

  for (auto& slt : m_slots)
    slt.cleanup(sc);

  if (sc)
    sc->removeItem(view);

  delete m_timebars;
  delete view;
}

void FullViewIntervalPresenter::createSlot(int slot_i, const FullSlot& slt)
{
  if(m_nodal)
    return;

  // Create the slot
  {
    SlotPresenter p;
    p.header = new SlotHeader{*this, slot_i, m_view};
    p.footer = new AmovibleSlotFooter{*this, slot_i, m_view};
    m_slots.insert(m_slots.begin() + slot_i, std::move(p));
  }

  const auto& proc = m_model.processes.at(slt.process);

  // Create a layer container
  auto& slot = m_slots.at(slot_i);
  auto& ld = slot.layers.emplace_back(&proc);

  // Create layers
  const auto factory = m_context.processList.findDefaultFactory(proc.concreteKey());

  const auto gui_width = m_model.duration.guiDuration().toPixels(m_zoomRatio);
  const auto def_width = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
  const auto slot_height = ld.model().getSlotHeight();
  ld.updateLoops(m_context, m_zoomRatio, gui_width, def_width, slot_height, m_view, this);

  {
    slot.headerDelegate = factory->makeHeaderDelegate(proc, m_context, ld.mainPresenter());
    slot.headerDelegate->setParentItem(slot.header);
    slot.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsToShape);
    slot.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsChildrenToShape);
    slot.headerDelegate->setPos(30, 0);
  }
  {
    slot.footerDelegate = factory->makeFooterDelegate(proc, m_context);
    slot.footerDelegate->setParentItem(slot.footer);
    slot.footerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsToShape);
    slot.footerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsChildrenToShape);
    slot.footerDelegate->setPos(30, 0);
  }

  // Connect loop things
  LayerData::disconnect(proc, *this);

  con(proc, &Process::ProcessModel::loopsChanged,
      this, [this, slot_i] (bool b) {
    SCORE_ASSERT(slot_i < int(m_slots.size()));
    auto& slt = this->m_slots[slot_i];

    SCORE_ASSERT(!slt.layers.empty());
    LayerData& ld = slt.layers.front();
    const auto gui_width = m_model.duration.guiDuration().toPixels(m_zoomRatio);
    const auto def_width = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
    const auto slot_height = ld.model().getSlotHeight();
    ld.updateLoops(m_context, m_zoomRatio, gui_width, def_width, slot_height, this->m_view, this);
  });

  con(proc, &Process::ProcessModel::startOffsetChanged,
      this, [this, slot_i] {
    SCORE_ASSERT(slot_i < int(m_slots.size()));
    auto& slot = this->m_slots[slot_i];

    SCORE_ASSERT(!slot.layers.empty());
    auto& ld = slot.layers.front();

    ld.updateStartOffset(-ld.model().startOffset().toPixels(m_zoomRatio));
  });
  con(proc, &Process::ProcessModel::loopDurationChanged,
      this, [this, slot_i] {
    SCORE_ASSERT(slot_i < int(m_slots.size()));
    auto& slt = this->m_slots[slot_i];

    SCORE_ASSERT(!slt.layers.empty());
    LayerData& ld = slt.layers.front();
    const auto gui_width = m_model.duration.guiDuration().toPixels(m_zoomRatio);
    const auto def_width = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
    const auto slot_height = ld.model().getSlotHeight();
    ld.updateLoops(m_context, m_zoomRatio, gui_width, def_width, slot_height, this->m_view, this);
  });

  updateProcessShape(slot_i);
}

void FullViewIntervalPresenter::requestSlotMenu(
    int slot,
    QPoint pos,
    QPointF sp) const
{
  if(m_nodal)
    return;

  auto menu = new QMenu;
  auto& reg = score::GUIAppContext()
                  .guiApplicationPlugin<ScenarioApplicationPlugin>()
                  .layerContextMenuRegistrar();

  m_slots[slot].layers.front().fillContextMenu(
      *menu, pos, sp, reg);
  menu->exec(pos);
  menu->close();
  menu->deleteLater();
}

void FullViewIntervalPresenter::updateProcessShape(
    const LayerData& ld,
    const SlotPresenter& slot)
{
  if(m_nodal)
    return;

  const auto h = ld.model().getSlotHeight();
  ld.setHeight(h);
  ld.updateContainerHeights(h); // TODO merge with setHeight

  auto width = m_model.duration.guiDuration().toPixels(m_zoomRatio);
  auto dwidth = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
  ld.setWidth(width, dwidth);

  slot.header->setWidth(width);
  slot.footer->setWidth(width);

  slot.headerDelegate->setSize(QSizeF{
      std::max(
          0., width - SlotHeader::handleWidth() - SlotHeader::menuWidth()),
      SlotHeader::headerHeight()});
  slot.headerDelegate->setX(30);
  slot.footerDelegate->setSize(QSizeF{width, SlotFooter::footerHeight()});
  slot.footerDelegate->setX(30);

  ld.parentGeometryChanged();
}

void FullViewIntervalPresenter::updateProcessShape(int slot)
{
  if(m_nodal)
    return;

  auto& slt = m_slots.at(slot);
  if (!slt.layers.empty())
    updateProcessShape(slt.layers.front(), slt);
}

void FullViewIntervalPresenter::on_slotRemoved(int pos)
{
  if(m_nodal)
    return;

  SlotPresenter& slot = m_slots.at(pos);
  for(const LayerData& proc : slot.layers)
  {
    proc.disconnect(proc.model(), *this);
  }
  slot.cleanup(this->view()->scene());

  m_slots.erase(m_slots.begin() + pos);
}

void FullViewIntervalPresenter::updateProcessesShape()
{
  if(m_nodal)
    return;

  for (int i = 0; i < (int)m_slots.size(); i++)
  {
    updateProcessShape(i);
  }
}

void FullViewIntervalPresenter::updatePositions()
{
  if(m_nodal)
    return;

  using namespace std;
  // Vertical shape
  m_view->setHeight(rackHeight() + IntervalHeader::headerHeight());

  // Set the slots position graphically in order.
  qreal currentSlotY = 2.;

  for (int i = 0; i < (int)m_slots.size(); i++)
  {
    SlotPresenter& slot = m_slots[i];
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
  }

  // Horizontal shape
  on_defaultDurationChanged(m_model.duration.defaultDuration());
  on_guiDurationChanged(m_model.duration.guiDuration());

  updateProcessesShape();
  updateHeight();
}

double FullViewIntervalPresenter::rackHeight() const
{
  if(m_nodal)
    return 0.;

  qreal height = 0;
  for (const SlotPresenter& slot : m_slots)
  {
    if (!slot.layers.empty())
      height += slot.layers.front().model().getSlotHeight();

    height += slot.headerHeight() + slot.footerHeight();
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

  delete m_nodal;
  m_nodal = nullptr;

  switch(m_model.viewMode())
  {
    case IntervalModel::ViewMode::Temporal:
    {
      // Recreate
      m_slots.reserve(m_model.fullView().size());

      int i = 0;
      for (const auto& slt : m_model.fullView())
      {
        createSlot(i, slt);
        i++;
      }

      break;
    }
    case IntervalModel::ViewMode::Nodal:
    {
      m_nodal = new NodalIntervalView{*this, m_context, m_view};
      return;
    }
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
  auto& slot = m_slots[i];

  m_context.focusDispatcher.focus(m_slots[i].headerDelegate->m_presenter);
  disp.setAndCommit({&slot.layers.front().model()});
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

  if(m_nodal)
  {
    m_nodal->on_zoomRatioChanged(ratio);
    return;
  }

  updateTimeBars();

  auto gui_width = m_model.duration.guiDuration().toPixels(ratio);
  auto def_width = m_model.duration.defaultDuration().toPixels(ratio);

  for (auto& slot : m_slots)
  {
    if (slot.headerDelegate)
      slot.headerDelegate->on_zoomRatioChanged(ratio);
    for (auto& ld : slot.layers)
    {
      const auto slot_height = ld.model().getSlotHeight();
      ld.on_zoomRatioChanged(m_context, ratio, gui_width, def_width, slot_height, m_view, this);
    }
  }

  updateProcessesShape();
}

void FullViewIntervalPresenter::requestModeChange(bool state)
{
  auto mode = state ? IntervalModel::ViewMode::Nodal : IntervalModel::ViewMode::Temporal;
  ((IntervalModel&)m_model).setViewMode(mode);
  on_modeChanged(mode);
}

double FullViewIntervalPresenter::on_playPercentageChanged(double t)
{
  if(m_nodal)
  {
    m_nodal->on_playPercentageChanged(ossia::clamp(t, 0., 1.));
  }
  return IntervalPresenter::on_playPercentageChanged(t);
}

void FullViewIntervalPresenter::on_visibleRectChanged(QRectF r)
{
  if(r != m_sceneRect)
  {
    m_sceneRect = r;
    updateTimeBars();
  }
}

void FullViewIntervalPresenter::updateTimeBars()
{
  // TODO we should use the interval view rect instead of the scene rect as reference

  TimeVal x0_time = TimeVal::fromMsecs(m_zoomRatio * m_sceneRect.x());
  TimeVal x1_time = TimeVal::fromMsecs(m_zoomRatio * (m_sceneRect.x() + m_sceneRect.width()));

  // Find the measure before x0_time

  const auto& measures = m_model.timeSignatureMap();
  auto last_before = ossia::last_before(measures, x0_time);

  qDebug() << x0_time // <<x1_time
           << last_before->second.upper;
  m_timebars->timebar.setZoomRatio(m_zoomRatio);

  double tempo = 120.;
  double whole = TimeVal(1000. * 240. / tempo).toPixels(m_zoomRatio);
  auto& lightBars = m_timebars->lightBars;
  auto& lighterBars = m_timebars->lighterBars;

  for(int i = 0; i < lighterBars.size(); i+=4)
  {
    lighterBars[i  ].setPos((i+1) * whole, 10.);
    lighterBars[i+1].setPos((i+2) * whole, 10.);
    lighterBars[i+2].setPos((i+3) * whole, 10.);
  }
  for(int i = 0; i < lightBars.size(); i++)
  {
    lightBars[i].setPos(4 * i * whole, 10.);
  }
}

void FullViewIntervalPresenter::on_modeChanged(IntervalModel::ViewMode m)
{
  on_rackChanged();
}

void FullViewIntervalPresenter::on_guiDurationChanged(const TimeVal& val)
{
  const auto gui_width = val.toPixels(m_zoomRatio);
  const auto def_width = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
  m_header->setWidth(gui_width);

  static_cast<FullViewIntervalView*>(m_view)->setGuiWidth(gui_width);

  if(m_nodal)
    return;

  for (SlotPresenter& slot : m_slots)
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
}

void FullViewIntervalPresenter::updateHeight()
{
  m_view->setHeight(rackHeight() + IntervalHeader::headerHeight());

  updateChildren();
  heightChanged();
}
}
