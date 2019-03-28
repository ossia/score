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
#include <Scenario/Document/Interval/SlotHandle.hpp>
#include <Scenario/Document/Interval/SlotHeader.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/graphics/GraphicsItem.hpp>

#include <QGraphicsScene>
#include <QList>
#include <QPainter>

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
      &IntervalHeader::setText);
  m_header->setText(metadata.getName());
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
  if (sc)
  {
    for (auto& slt : m_slots)
    {
      if (slt.header)
      {
        sc->removeItem(slt.header);
        delete slt.header;
      }
      if (slt.handle)
      {
        sc->removeItem(slt.handle);
        delete slt.handle;
      }
      for (auto& layer : slt.processes)
      {
        // The presenter will delete their views
        delete layer.presenter;
      }
    }

    sc->removeItem(view);
  }
  else
  {
    for (auto& slt : m_slots)
    {
      if (slt.header)
      {
        delete slt.header;
      }
      if (slt.handle)
      {
        delete slt.handle;
      }
      for (auto& layer : slt.processes)
      {
        // The presenter will delete their views
        delete layer.presenter;
      }
    }
  }

  delete view;
}

void FullViewIntervalPresenter::createSlot(int pos, const FullSlot& slt)
{
  SlotPresenter p;
  p.header = new SlotHeader{*this, pos, m_view};
  p.handle = new SlotHandle{*this, pos, false, m_view};

  const auto& proc = m_model.processes.at(slt.process);

  const auto& procKey = proc.concreteKey();

  auto factory = m_context.processList.findDefaultFactory(procKey);
  auto proc_view = factory->makeLayerView(proc, m_view);
  auto proc_pres
      = factory->makeLayerPresenter(proc, proc_view, m_context, this);
  proc_pres->putToFront();
  proc_pres->on_zoomRatioChanged(m_zoomRatio);
  proc_pres->setFullView();

  p.headerDelegate = factory->makeHeaderDelegate(*proc_pres);
  p.headerDelegate->setParentItem(p.header);
  p.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsToShape);
  p.headerDelegate->setFlag(
      QGraphicsItem::GraphicsItemFlag::ItemClipsChildrenToShape);
  p.headerDelegate->setPos(30, 0);

  m_slots.insert(m_slots.begin() + pos, std::move(p));
  auto& slot = m_slots.at(pos);
  slot.processes.push_back(LayerData{&proc, proc_pres, proc_view});

  auto con_id = con(
      proc,
      &Process::ProcessModel::durationChanged,
      this,
      [&](const TimeVal&) {
        int i = 0;
        auto it = ossia::find_if(m_slots, [&](const SlotPresenter& elt) {
          return elt.processes.front().model->id() == proc.id();
        });
        if (it != m_slots.end())
          updateProcessShape(it->processes.front(), *it);
        i++;
      });

  con(proc,
      &IdentifiedObjectAbstract::identified_object_destroying,
      this,
      [=] { QObject::disconnect(con_id); });

  updateProcessShape(pos);
}

void FullViewIntervalPresenter::updateProcessShape(
    const LayerData& data,
    const SlotPresenter& slot)
{
  data.presenter->setHeight(data.model->getSlotHeight());

  auto width = m_model.duration.guiDuration().toPixels(m_zoomRatio);
  data.presenter->setWidth(width);

  slot.header->setWidth(width);

  slot.headerDelegate->setSize(QSizeF{
      std::max(
          0., width - SlotHeader::handleWidth() - SlotHeader::menuWidth()),
      slot.header->headerHeight()});
  slot.headerDelegate->setX(30);

  data.presenter->parentGeometryChanged();
}

void FullViewIntervalPresenter::updateProcessShape(int slot)
{
  auto& slt = m_slots.at(slot);
  if (!slt.processes.empty())
    updateProcessShape(slt.processes.front(), slt);
}

void FullViewIntervalPresenter::on_slotRemoved(int pos)
{
  SlotPresenter& slot = m_slots.at(pos);
  if (!slot.processes.empty())
  {
    QPointer<Process::LayerView> view_p{slot.processes.front().view};
    delete slot.processes.front().presenter;
    if (view_p)
      deleteGraphicsItem(slot.processes.front().view);
  }
  deleteGraphicsItem(slot.header);
  slot.header = nullptr;
  deleteGraphicsItem(slot.handle);
  slot.handle = nullptr;

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
    const SlotPresenter& slot = m_slots[i];
    if (!slot.processes.empty())
    {
      const LayerData& proc = slot.processes.front();

      assert(slot.header);
      if (slot.header)
      {
        slot.header->setPos(QPointF{0, currentSlotY});
        slot.header->setSlotIndex(i);
      }
      currentSlotY += slot.headerHeight();

      proc.view->setPos(0, currentSlotY);
      proc.view->update();

      currentSlotY += proc.model->getSlotHeight();

      slot.handle->setPos(0, currentSlotY);
      slot.handle->setSlotIndex(i);
      currentSlotY += SlotHandle::handleHeight();
    }
    else
    {
      currentSlotY += SlotHeader::headerHeight();
      currentSlotY += SlotHandle::handleHeight();
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
    if (!slot.processes.empty())
      height += slot.processes.front().model->getSlotHeight();

    height += SlotHandle::handleHeight() + slot.headerHeight();
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

  m_context.focusDispatcher.focus(m_slots[i].headerDelegate->presenter);
  disp.setAndCommit({slot.processes.front().model});
}

void FullViewIntervalPresenter::on_defaultDurationChanged(const TimeVal& val)
{
  const auto w = val.toPixels(m_zoomRatio);
  m_view->setDefaultWidth(w);
  m_view->updateLabelPos();
  m_view->updateCounterPos();
  ((FullViewIntervalView*)m_view)->updateOverlayPos();

  for (const SlotPresenter& slot : m_slots)
  {
    slot.header->setWidth(w);
    if (slot.handle)
      slot.handle->setWidth(w);
    if (slot.headerDelegate)
      slot.headerDelegate->setSize(
          QSizeF{w - SlotHeader::handleWidth() - SlotHeader::menuWidth(),
                 SlotHeader::headerHeight()});
    for (const LayerData& proc : slot.processes)
    {
      proc.presenter->setWidth(w);
    }
  }

  m_header->update();
  m_view->update();
}

void FullViewIntervalPresenter::on_zoomRatioChanged(ZoomRatio val)
{
  IntervalPresenter::on_zoomRatioChanged(val);

  for (const SlotPresenter& slot : m_slots)
  {
    if (slot.headerDelegate)
      slot.headerDelegate->on_zoomRatioChanged(val);
    for (auto proc : slot.processes)
      proc.presenter->on_zoomRatioChanged(val);
  }

  updateProcessesShape();
}

int FullViewIntervalPresenter::indexOfSlot(const Process::LayerPresenter& p)
{
  for (int i = 0; i < (int)m_slots.size(); ++i)
  {
    if (m_slots[i].processes.front().presenter == &p)
      return i;
  }

  SCORE_ABORT;
}

void FullViewIntervalPresenter::on_guiDurationChanged(const TimeVal& val)
{
  const auto w = val.toPixels(m_zoomRatio);
  m_header->setWidth(w);

  static_cast<FullViewIntervalView*>(m_view)->setGuiWidth(w);
  for (const SlotPresenter& slot : m_slots)
  {
    slot.handle->setWidth(w);
    if (!slot.processes.empty())
      slot.processes.front().presenter->setWidth(w);
  }
}

void FullViewIntervalPresenter::updateHeight()
{
  m_view->setHeight(rackHeight() + IntervalHeader::headerHeight());

  updateChildren();
  heightChanged();
}
}
