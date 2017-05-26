#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>
#include <QGraphicsScene>
#include <QList>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/FullView/FullViewConstraintView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include <Scenario/Document/Constraint/SlotHandle.hpp>
#include <Process/LayerView.hpp>
#include "AddressBarItem.hpp"
#include "FullViewConstraintHeader.hpp"
#include "FullViewConstraintPresenter.hpp"
#include <Scenario/Document/Constraint/ConstraintPresenter.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <iscore/widgets/GraphicsItem.hpp>

class QObject;

namespace Scenario
{
FullViewConstraintPresenter::FullViewConstraintPresenter(
    const ConstraintModel& constraint,
    const Process::ProcessPresenterContext& ctx,
    QGraphicsItem* parentobject,
    QObject* parent)
    : ConstraintPresenter{
          constraint, new FullViewConstraintView{*this, parentobject},
          new FullViewConstraintHeader{parentobject}, ctx, parent}
{
  // Address bar
  auto addressBar = static_cast<FullViewConstraintHeader*>(m_header)->bar();
  addressBar->setTargetObject(iscore::IDocument::unsafe_path(constraint));
  connect(addressBar, &AddressBarItem::constraintSelected, this,
          &FullViewConstraintPresenter::constraintSelected);

  // Header
  const auto& metadata = m_model.metadata();
  con(metadata, &iscore::ModelMetadata::NameChanged, m_header,
      &ConstraintHeader::setText);
  m_header->setText(metadata.getName());
  m_header->show();

  // Time
  con(constraint.duration, &ConstraintDurations::defaultDurationChanged, this,
      [&](const TimeVal& val) {
    on_defaultDurationChanged(val);
  });
  con(constraint.duration, &ConstraintDurations::guiDurationChanged, this,
      [&](const TimeVal& val) {
        on_guiDurationChanged(val);
        updateChildren();
      });

  // Slots
  con(m_model, &ConstraintModel::rackChanged,
      this, [=] (Slot::RackView t) {
    if(t == Slot::FullView)
      on_rackChanged();
  });

  con(m_model, &ConstraintModel::slotAdded,
      this, [=] (const SlotId& s) {
    if(s.fullView())
      on_rackChanged();
  });

  con(m_model, &ConstraintModel::slotRemoved,
      this, [=] (const SlotId& s) {
    if(s.fullView())
      on_rackChanged();
  });

  con(m_model, &ConstraintModel::slotResized,
          this, [this] (const SlotId& s) {
    if(s.fullView())
      this->updatePositions();
  });

  // Initial state
  const auto& slts = constraint.fullView();
  m_slots.reserve(slts.size());
  on_rackChanged();
}

FullViewConstraintPresenter::~FullViewConstraintPresenter()
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

void FullViewConstraintPresenter::createSlot(int pos, const FullSlot& slt)
{
  SlotPresenter p;
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

  m_slots.at(pos).process = LayerData{
                  &proc, proc_pres, proc_view
                };

  auto con_id = con(
        proc, &Process::ProcessModel::durationChanged, this,
        [&] (const TimeVal&) {
    int i = 0;
    auto it = ossia::find_if(m_slots,
                             [&] (const SlotPresenter& elt) {
      return elt.process.model->id() == proc.id();
    });
    if(it != m_slots.end())
        updateProcessShape(i);
      i++;

  });

  con(proc, &IdentifiedObjectAbstract::identified_object_destroying, this,
      [=] { QObject::disconnect(con_id); });

  updateProcessShape(pos);
}

void FullViewConstraintPresenter::updateProcessShape(int slot)
{
  const LayerData& data = m_slots.at(slot).process;
  data.presenter->setHeight(data.model->getSlotHeight());

  auto width = m_model.duration.guiDuration().toPixels(m_zoomRatio);
  data.presenter->setWidth(width);
  data.presenter->parentGeometryChanged();
}

void FullViewConstraintPresenter::on_slotRemoved(int pos)
{
  SlotPresenter& slot = m_slots.at(pos);

  QPointer<Process::LayerView> view_p{slot.process.view};
  delete slot.process.presenter;
  if (view_p)
    deleteGraphicsItem(slot.process.view);

  deleteGraphicsItem(slot.handle);

  m_slots.erase(m_slots.begin() + pos);
}

void FullViewConstraintPresenter::updateProcessesShape()
{
  for(int i = 0; i < (int)m_slots.size(); i++)
  {
    updateProcessShape(i);
  }
  updateHeight();
}

void FullViewConstraintPresenter::updatePositions()
{
  using namespace std;
  // Vertical shape
  m_view->setHeight(rackHeight() + ConstraintHeader::headerHeight());

  // Set the slots position graphically in order.
  qreal currentSlotY = ConstraintHeader::headerHeight();

  for(int i = 0; i < (int)m_slots.size(); i++)
  {
    const SlotPresenter& slot = m_slots[i];
    const LayerData& proc = slot.process;

    proc.view->setPos(0, currentSlotY);
    proc.view->update();

    currentSlotY += proc.model->getSlotHeight();

    slot.handle->setPos(0, currentSlotY);
    currentSlotY += SlotHandle::handleHeight();
  }

  // Horizontal shape
  on_defaultDurationChanged(m_model.duration.defaultDuration());
  on_guiDurationChanged(m_model.duration.guiDuration());

  updateProcessesShape();
}

double FullViewConstraintPresenter::rackHeight() const
{
  qreal height = 0;
  for(const SlotPresenter& slot : m_slots)
  {
    height += slot.process.model->getSlotHeight() + SlotHandle::handleHeight();
  }
  return height;
}

void FullViewConstraintPresenter::on_rackChanged()
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

void FullViewConstraintPresenter::updateScaling()
{
  on_defaultDurationChanged(model().duration.defaultDuration());
  on_guiDurationChanged(model().duration.guiDuration());
  ConstraintPresenter::updateScaling();
  updateHeight();
}

void FullViewConstraintPresenter::on_defaultDurationChanged(const TimeVal& val)
{
  const auto w = val.toPixels(m_zoomRatio);
  m_header->setWidth(w);
  m_view->setDefaultWidth(w);
  m_view->updateLabelPos();
  m_view->updateCounterPos();
  m_view->updateOverlayPos();

  m_header->update();
  m_view->update();
}

void FullViewConstraintPresenter::on_zoomRatioChanged(ZoomRatio val)
{
  ConstraintPresenter::on_zoomRatioChanged(val);

  for(const SlotPresenter& slot : m_slots)
  {
    slot.process.presenter->on_zoomRatioChanged(val);
  }

  updateProcessesShape();
}

int FullViewConstraintPresenter::indexOfSlot(const Process::LayerPresenter& p)
{
  for(int i = 0; i < (int)m_slots.size(); ++i)
  {
    if(m_slots[i].process.presenter == &p)
      return i;
  }

  ISCORE_ABORT;
}

void FullViewConstraintPresenter::on_guiDurationChanged(const TimeVal& val)
{
  const auto w = val.toPixels(m_zoomRatio);

  static_cast<FullViewConstraintView*>(m_view)->setGuiWidth(w);
  for(const SlotPresenter& slot : m_slots)
  {
    slot.handle->setWidth(w);
    slot.process.presenter->setWidth(w);
  }
}

void FullViewConstraintPresenter::updateHeight()
{
  m_view->setHeight(rackHeight() + ConstraintHeader::headerHeight());

  updateChildren();
  emit heightChanged();
}
}
