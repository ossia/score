// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>
#include <QGraphicsScene>
#include <QList>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalView.hpp>
#include <Scenario/Document/Interval/DefaultHeaderDelegate.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <score/document/DocumentInterface.hpp>

#include <Scenario/Document/Interval/SlotHandle.hpp>
#include <Scenario/Document/Interval/SlotHeader.hpp>
#include <Process/LayerView.hpp>
#include "AddressBarItem.hpp"
#include "FullViewIntervalHeader.hpp"
#include "FullViewIntervalPresenter.hpp"
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <score/widgets/GraphicsItem.hpp>

class QObject;

namespace Scenario
{
FullViewIntervalPresenter::FullViewIntervalPresenter(
    const IntervalModel& interval,
    const Process::ProcessPresenterContext& ctx,
    QGraphicsItem* parentobject,
    QObject* parent)
    : IntervalPresenter{
          interval, new FullViewIntervalView{*this, parentobject},
          new FullViewIntervalHeader{ctx, parentobject}, ctx, parent}
{
  // Address bar
  auto& addressBar = static_cast<FullViewIntervalHeader*>(m_header)->bar();
  addressBar.setTargetObject(score::IDocument::unsafe_path(interval));
  con(addressBar, &AddressBarItem::intervalSelected, this,
          &FullViewIntervalPresenter::intervalSelected);

  con(interval.selection, &Selectable::changed, (FullViewIntervalView*)m_view,
      &FullViewIntervalView::setSelected);

  // Header
  const auto& metadata = m_model.metadata();
  con(metadata, &score::ModelMetadata::NameChanged, m_header,
      &IntervalHeader::setText);
  m_header->setText(metadata.getName());
  m_header->show();

  // Time
  con(interval.duration, &IntervalDurations::defaultDurationChanged, this,
      [&](const TimeVal& val) {
    on_defaultDurationChanged(val);
  });
  con(interval.duration, &IntervalDurations::guiDurationChanged, this,
      [&](const TimeVal& val) {
        on_guiDurationChanged(val);
        updateChildren();
      });

  // Slots
  con(m_model, &IntervalModel::rackChanged,
      this, [=] (Slot::RackView t) {
    if(t == Slot::FullView)
      on_rackChanged();
  });

  con(m_model, &IntervalModel::slotAdded,
      this, [=] (const SlotId& s) {
    if(s.fullView())
      on_rackChanged();
  });

  con(m_model, &IntervalModel::slotRemoved,
      this, [=] (const SlotId& s) {
    if(s.fullView())
      on_rackChanged();
  });

  con(m_model, &IntervalModel::slotResized,
          this, [this] (const SlotId& s) {
    if(s.fullView())
      this->updatePositions();
  });

  // Initial state
  const auto& slts = interval.fullView();
  m_slots.reserve(slts.size());
  on_rackChanged();
}

FullViewIntervalPresenter::~FullViewIntervalPresenter()
{
  // TODO deleteGraphicsObject ?
  if (Scenario::view(this))
  {
    auto sc = Scenario::view(this)->scene();

    if (sc && sc->items().contains(Scenario::view(this)))
    {
      sc->removeItem(Scenario::view(this));
    }

    Scenario::view(this)->deleteLater();
  }
}

void FullViewIntervalPresenter::createSlot(int pos, const FullSlot& slt)
{
  SlotPresenter p;
  p.header = new SlotHeader{*this, pos, m_view};
  p.handle = new SlotHandle{*this, pos, m_view};
  m_slots.insert(m_slots.begin() + pos, std::move(p));

  const auto& proc = m_model.processes.at(slt.process);

  const auto& procKey = proc.concreteKey();

  auto factory = m_context.processList.findDefaultFactory(procKey);
  auto proc_view = factory->makeLayerView(proc, m_view);
  auto proc_pres = factory->makeLayerPresenter(proc, proc_view, m_context, this);
  proc_pres->putToFront();
  proc_pres->on_zoomRatioChanged(m_zoomRatio);
  proc_pres->setFullView();

  auto& slot = m_slots.at(pos);

  slot.processes.push_back(LayerData{
                  &proc, proc_pres, proc_view
                });
  slot.headerDelegate =
      new DefaultHeaderDelegate{*proc_pres};
  slot.headerDelegate->setParentItem(slot.header);
  slot.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsToShape);
  slot.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsChildrenToShape);
  slot.headerDelegate->setPos(30, 0);

  auto con_id = con(
        proc, &Process::ProcessModel::durationChanged, this,
        [&] (const TimeVal&) {
    int i = 0;
    auto it = ossia::find_if(m_slots,
                             [&] (const SlotPresenter& elt) {
      return elt.processes.front().model->id() == proc.id();
    });
    if(it != m_slots.end())
        updateProcessShape(it->processes.front());
    i++;

  });

  con(proc, &IdentifiedObjectAbstract::identified_object_destroying, this,
      [=] { QObject::disconnect(con_id); });

  updateProcessShape(pos);
}

void FullViewIntervalPresenter::updateProcessShape(const LayerData& data)
{
  data.presenter->setHeight(data.model->getSlotHeight());

  auto width = m_model.duration.guiDuration().toPixels(m_zoomRatio);
  data.presenter->setWidth(width);
  data.presenter->parentGeometryChanged();
}
void FullViewIntervalPresenter::updateProcessShape(int slot)
{
  updateProcessShape(m_slots.at(slot).processes.front());
}

void FullViewIntervalPresenter::on_slotRemoved(int pos)
{
  SlotPresenter& slot = m_slots.at(pos);

  QPointer<Process::LayerView> view_p{slot.processes.front().view};
  delete slot.processes.front().presenter;
  if (view_p)
    deleteGraphicsItem(slot.processes.front().view);

  deleteGraphicsItem(slot.handle);

  m_slots.erase(m_slots.begin() + pos);
}

void FullViewIntervalPresenter::updateProcessesShape()
{
  for(int i = 0; i < (int)m_slots.size(); i++)
  {
    updateProcessShape(i);
  }
  updateHeight();
}

void FullViewIntervalPresenter::updatePositions()
{
  using namespace std;
  // Vertical shape
  m_view->setHeight(rackHeight() + IntervalHeader::headerHeight());

  // Set the slots position graphically in order.
  qreal currentSlotY = IntervalHeader::headerHeight();

  for(int i = 0; i < (int)m_slots.size(); i++)
  {
    const SlotPresenter& slot = m_slots[i];
    const LayerData& proc = slot.processes.front();

    slot.header->setPos(QPointF{0, currentSlotY});
    slot.header->setSlotIndex(i);

    proc.view->setPos(0, currentSlotY);
    proc.view->update();

    currentSlotY += proc.model->getSlotHeight();

    slot.handle->setPos(0, currentSlotY);
    slot.handle->setSlotIndex(i);
    currentSlotY += SlotHandle::handleHeight();
  }

  // Horizontal shape
  on_defaultDurationChanged(m_model.duration.defaultDuration());
  on_guiDurationChanged(m_model.duration.guiDuration());

  updateProcessesShape();
}

double FullViewIntervalPresenter::rackHeight() const
{
  qreal height = 0;
  for(const SlotPresenter& slot : m_slots)
  {
    height += slot.processes.front().model->getSlotHeight() + SlotHandle::handleHeight();
  }
  return height;
}

void FullViewIntervalPresenter::on_rackChanged()
{
  // Remove existing
  for(int i = m_slots.size(); i --> 0 ; )
  {
    on_slotRemoved(i);
  }

  // Recreate
  m_slots.reserve(m_model.fullView().size());

  int i = 0;
  for(const auto& slt : m_model.fullView())
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

void FullViewIntervalPresenter::on_defaultDurationChanged(const TimeVal& val)
{
  const auto w = val.toPixels(m_zoomRatio);
  m_view->setDefaultWidth(w);
  m_view->updateLabelPos();
  m_view->updateCounterPos();
  ((FullViewIntervalView*)m_view)->updateOverlayPos();

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

  m_header->update();
  m_view->update();
}

void FullViewIntervalPresenter::on_zoomRatioChanged(ZoomRatio val)
{
  IntervalPresenter::on_zoomRatioChanged(val);

  for(const SlotPresenter& slot : m_slots)
  {
    slot.processes.front().presenter->on_zoomRatioChanged(val);
  }

  updateProcessesShape();
}

int FullViewIntervalPresenter::indexOfSlot(const Process::LayerPresenter& p)
{
  for(int i = 0; i < (int)m_slots.size(); ++i)
  {
    if(m_slots[i].processes.front().presenter == &p)
      return i;
  }

  SCORE_ABORT;
}

void FullViewIntervalPresenter::on_guiDurationChanged(const TimeVal& val)
{
  const auto w = val.toPixels(m_zoomRatio);
  m_header->setWidth(w);

  static_cast<FullViewIntervalView*>(m_view)->setGuiWidth(w);
  for(const SlotPresenter& slot : m_slots)
  {
    slot.handle->setWidth(w);
    slot.processes.front().presenter->setWidth(w);
  }
}

void FullViewIntervalPresenter::updateHeight()
{
  m_view->setHeight(rackHeight() + IntervalHeader::headerHeight());

  updateChildren();
  emit heightChanged();
}
}
