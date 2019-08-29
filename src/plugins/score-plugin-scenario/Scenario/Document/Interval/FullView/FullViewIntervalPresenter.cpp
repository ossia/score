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
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/tools/Bind.hpp>

#include <QGraphicsScene>
#include <QMenu>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::FullViewIntervalPresenter)
namespace Scenario
{

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
    const Process::ProcessPresenterContext& ctx,
    QGraphicsItem* parentobject,
    QObject* parent)
    : IntervalPresenter{interval,
                        new FullViewIntervalView{*this, parentobject},
                        new FullViewIntervalHeader{ctx, parentobject},
                        ctx,
                        parent}
{
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

  // Header
  const auto& metadata = m_model.metadata();
  con(metadata,
      &score::ModelMetadata::NameChanged,
      m_header,
      &IntervalHeader::on_textChanged);
  m_header->on_textChanged();
  m_header->show();

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

  con(m_model,
      &IntervalModel::slotsSwapped,
      this,
      [=](int i, int j, Slot::RackView v) {
        if (v == Slot::FullView)
          on_rackChanged();
      });

  con(m_model, &IntervalModel::slotResized, this, [this](const SlotId& s) {
    if (s.fullView())
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
  const auto& slts = interval.fullView();
  m_slots.reserve(slts.size());
  on_rackChanged();
}

FullViewIntervalPresenter::~FullViewIntervalPresenter()
{
  auto view = Scenario::view(this);
  auto sc = view->scene();
  for (auto& slt : m_slots)
  {
    slt.cleanup(sc);
  }

  if (sc)
  {
    sc->removeItem(view);
  }

  delete view;
}

void FullViewIntervalPresenter::createSlot(int pos, const FullSlot& slt)
{
  // Create the slot
  {
    SlotPresenter p;
    p.header = new SlotHeader{*this, pos, m_view};
    p.footer = new AmovibleSlotFooter{*this, pos, m_view};
    m_slots.insert(m_slots.begin() + pos, std::move(p));
  }

  const auto& proc = m_model.processes.at(slt.process);
  const auto& procKey = proc.concreteKey();

  auto factory = m_context.processList.findDefaultFactory(procKey);

  // TODO disconnect when the slot is removed
  con(proc, &Process::ProcessModel::loopsChanged,
      this, [this, pos, factory] (bool b) {
    SCORE_ASSERT(pos < m_slots.size());
    auto& slt = this->m_slots[pos];

    SCORE_ASSERT(!slt.layers.empty());
    LayerData& ld = slt.layers.front();

    if(b)
    {
      auto view_width = ld.model().loopDuration().toPixels(m_zoomRatio);
      auto interval_width = m_model.duration.guiDuration().toPixels(m_zoomRatio);
      auto individual_view_width = interval_width / view_width;
      auto view_max_count = std::max((int)individual_view_width, 1);

      for(int i = ld.count(); i < view_max_count; i++)
      {
        ld.addView(*factory, m_zoomRatio, m_context, this->m_view, this);
        ld.layers().back().container->setX(interval_width * i);
      }
    }
    else
    {
      for(int i = ld.count() - 1; i > 0; i--)
        ld.removeView(i);
    }
  });

  con(proc, &Process::ProcessModel::startOffsetChanged,
      this, [this, pos] {
    SCORE_ASSERT(pos < m_slots.size());
    auto& slot = this->m_slots[pos];

    SCORE_ASSERT(!slot.layers.empty());
    auto& ld = slot.layers.front();

    ld.updateStartOffset(-ld.model().startOffset().toPixels(m_zoomRatio));
  });
  con(proc, &Process::ProcessModel::loopDurationChanged,
      this, [this, pos] {
    SCORE_ASSERT(pos < m_slots.size());
    auto& slot = this->m_slots[pos];

    SCORE_ASSERT(!slot.layers.empty());
    auto& ld = slot.layers.front();
    auto view_width = ld.model().loopDuration().toPixels(m_zoomRatio);
    ld.updateXPositions(view_width);
    ld.updateContainerWidths(view_width);
  });

  // Create a layer container
  auto& slot = m_slots.at(pos);
  slot.layers.push_back(LayerData{&proc});
  auto& ld = slot.layers.back();

  // Create layers
  int view_max_count = 1;
  if(proc.loops())
  {
    auto view_width = proc.loopDuration().toPixels(m_zoomRatio);
    auto interval_width = m_model.duration.guiDuration().toPixels(m_zoomRatio);
    auto individual_view_width = interval_width / view_width;
    view_max_count = std::max((int)individual_view_width, 1);
  }

  for(int i = 0; i < view_max_count; i++)
  {
    ld.addView(*factory, m_zoomRatio, m_context, this->m_view, this);
  }

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


  auto con_id = con(
      proc,
      &Process::ProcessModel::durationChanged,
      this,
      [&](const TimeVal&) {
        int i = 0;
        auto it = ossia::find_if(m_slots, [&](const SlotPresenter& elt) {
          return elt.layers.front().model().id() == proc.id();
        });
        if (it != m_slots.end())
          updateProcessShape(it->layers.front(), *it);
        i++;
      });

  con(proc,
      &IdentifiedObjectAbstract::identified_object_destroying,
      this,
      [=] { QObject::disconnect(con_id); });

  updateProcessShape(pos);
}

void FullViewIntervalPresenter::requestSlotMenu(
    int slot,
    QPoint pos,
    QPointF sp) const
{
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
  auto& slt = m_slots.at(slot);
  if (!slt.layers.empty())
    updateProcessShape(slt.layers.front(), slt);
}

void FullViewIntervalPresenter::on_slotRemoved(int pos)
{
  SlotPresenter& slot = m_slots.at(pos);
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
  qreal currentSlotY = IntervalHeader::headerHeight();

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

      if(ld.model().loops())
      {
        auto view_width = ld.model().loopDuration().toPixels(m_zoomRatio);
        ld.updatePositions(currentSlotY, view_width);
      }
      else
      {
        ld.updatePositions(currentSlotY, 0);
      }

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

  for (const SlotPresenter& slot : m_slots)
  {
    slot.header->setWidth(w);
    slot.footer->setWidth(w);
    if (slot.headerDelegate)
      slot.headerDelegate->setSize(
          QSizeF{w - SlotHeader::handleWidth() - SlotHeader::menuWidth(),
                 SlotHeader::headerHeight()});
    for (const LayerData& ld : slot.layers)
    {
      ld.setWidth(w, w);
    }
  }

  m_header->update();
  m_view->update();
}

void FullViewIntervalPresenter::on_zoomRatioChanged(ZoomRatio ratio)
{
  IntervalPresenter::on_zoomRatioChanged(ratio);
  auto guiWidth = m_model.duration.guiDuration().toPixels(ratio);

  for (auto& slot : m_slots)
  {
    if (slot.headerDelegate)
      slot.headerDelegate->on_zoomRatioChanged(ratio);
    for (auto& proc : slot.layers)
    {
      proc.on_zoomRatioChanged(m_context, ratio, guiWidth, m_view, this);
    }
  }

  updateProcessesShape();
}

void FullViewIntervalPresenter::on_guiDurationChanged(const TimeVal& val)
{
  const auto w = val.toPixels(m_zoomRatio);
  m_header->setWidth(w);

  static_cast<FullViewIntervalView*>(m_view)->setGuiWidth(w);
  for (SlotPresenter& slot : m_slots)
  {
    if (!slot.layers.empty())
    {
      slot.layers.front().setWidth(w, w);
      slot.layers.front().updateLoops(m_context, m_zoomRatio, w, m_view, this);
    }
    slot.header->setWidth(w);
    slot.footer->setWidth(w);
  }
}

void FullViewIntervalPresenter::updateHeight()
{
  m_view->setHeight(rackHeight() + IntervalHeader::headerHeight());

  updateChildren();
  heightChanged();
}
}
