#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>
#include <QGraphicsScene>
#include <QList>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackView.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include <Scenario/Document/Constraint/Rack/RackPresenter.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotPresenter.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotHandle.hpp>
#include <Process/LayerView.hpp>
#include "AddressBarItem.hpp"
#include "FullViewConstraintHeader.hpp"
#include "FullViewConstraintPresenter.hpp"
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <iscore/widgets/GraphicsItem.hpp>

class QObject;

namespace Scenario
{
static const constexpr int slotSpacing = 10;
FullViewConstraintPresenter::FullViewConstraintPresenter(
    const ConstraintModel& cstr_model,
    const Process::ProcessPresenterContext& ctx,
    QGraphicsItem* parentobject,
    QObject* parent)
    : ConstraintPresenter{
          cstr_model, new FullViewConstraintView{*this, parentobject},
          new FullViewConstraintHeader{parentobject}, ctx, parent}
{
  // Update the address bar
  auto addressBar = static_cast<FullViewConstraintHeader*>(m_header)->bar();
  addressBar->setTargetObject(iscore::IDocument::unsafe_path(cstr_model));
  connect(
      addressBar, &AddressBarItem::constraintSelected, this,
      &FullViewConstraintPresenter::constraintSelected);

  const auto& metadata = m_model.metadata();
  con(metadata, &iscore::ModelMetadata::NameChanged, m_header,
      &ConstraintHeader::setText);
  m_header->setText(metadata.getName());
  m_header->show();

  const auto& slts = cstr_model.fullView();
  m_slots.reserve(slts.size());
  for(int i = 0; i < slts.size(); i++)
  {
    createSlot(i, slts[i]);
  }

  con(m_model, &ConstraintModel::rackChanged,
      this, [=] (Slot::RackView t) {
    if(t == Slot::FullView)
    {
      for(int i = slts.size(); i --> 0 ; i++)
      {
        on_slotRemoved(i);
      }
    }
  });
  con(m_model, &ConstraintModel::slotAdded,
      this, [=] (const SlotId& s) {
    createSlot(s.index, m_model.fullView()[s.index]);
  });

  con(m_model, &ConstraintModel::slotRemoved,
      this, [=] (const SlotId& s) {
    on_slotRemoved(s.index);
  });

  con(m_model, &ConstraintModel::slotResized,
          this, [this] (const auto& slotid) {
    this->updatePositions();
  });


  updatePositions();
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
  // p.view = new SlotView{};
  m_slots.insert(m_slots.begin() + pos, std::move(p));

  const auto& proc = m_model.processes.at(slt.process);

  const auto& procKey = proc.concreteKey();

  auto factory = m_context.processList.findDefaultFactory(procKey);
  auto proc_view = factory->makeLayerView(proc, m_view);
  auto proc_pres = factory->makeLayerPresenter(proc, proc_view, m_context, this);
  proc_pres->putToFront();
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
  data.presenter->setHeight(data.model->getSlotHeight() - SlotHandle::handleHeight());

  auto width = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
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
  //deleteGraphicsItem(slot.view);

  m_slots.erase(m_slots.begin() + pos);
}

void FullViewConstraintPresenter::updateProcessesShape()
{
  for(int i = 0; i < m_slots.size(); i++)
  {
    updateProcessShape(i);
  }
}

void FullViewConstraintPresenter::updatePositions()
{
  using namespace std;
  // Vertical shape
  m_view->setHeight(rackHeight() + ConstraintHeader::headerHeight());

  // Set the slots position graphically in order.
  qreal currentSlotY = ConstraintHeader::headerHeight();

  for(int i = 0; i < m_slots.size(); i++)
  {
    const SlotPresenter& slot = m_slots[i];
    const LayerData& proc = slot.process;

    proc.view->setPos(0, currentSlotY);
    proc.view->update();

    currentSlotY += proc.model->getSlotHeight() + slotSpacing; // Separation between slots
    slot.handle->setPos(0, currentSlotY - slotSpacing / 2);
  }

  // Horizontal shape
  on_defaultDurationChanged(m_model.duration.defaultDuration());

  updateProcessesShape();
}

double FullViewConstraintPresenter::rackHeight() const
{
  qreal height = 0;
  for(const SlotPresenter& slot : m_slots)
  {
    height += slot.process.model->getSlotHeight() + slotSpacing;
  }
  return height;
}

void FullViewConstraintPresenter::updateScaling()
{
  ConstraintPresenter::updateScaling();
  updateHeight();
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

void FullViewConstraintPresenter::on_defaultDurationChanged(const TimeVal& v)
{
  ConstraintPresenter::on_defaultDurationChanged(v);

  const auto w = m_view->defaultWidth();
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
