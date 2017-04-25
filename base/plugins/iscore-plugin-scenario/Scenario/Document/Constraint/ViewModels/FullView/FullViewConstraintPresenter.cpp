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

  con(m_model, &ConstraintModel::layerAdded,
      this, [=] (SlotId id, Id<Process::ProcessModel> proc) {
    createLayer(id.index, m_model.processes.at(proc));
  });
  con(m_model, &ConstraintModel::layerRemoved,
      this, [=] (SlotId, Id<Process::ProcessModel> proc) {
    removeLayer(m_model.processes.at(proc));
  });
  con(m_model, &ConstraintModel::frontLayerChanged,
      this, [=] (SlotId, OptionalId<Process::ProcessModel>) {

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

void FullViewConstraintPresenter::createSlot(int pos, const Slot& slt)
{
  SlotPresenter p;
  p.handle = new SlotHandle{*this, pos, m_view};
  // p.view = new SlotView{};
  m_slots.insert(m_slots.begin() + pos, std::move(p));

  for(const auto& process : slt.processes)
  {
    createLayer(pos, m_model.processes.at(process));
  }
}

void FullViewConstraintPresenter::createLayer(int slot, const Process::ProcessModel& proc)
{
  const auto& procKey = proc.concreteKey();

  auto factory = m_context.processList.findDefaultFactory(procKey);
  auto proc_view = factory->makeLayerView(proc, m_view);
  auto proc_pres = factory->makeLayerPresenter(proc, proc_view, m_context, this);
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

  auto frontLayer = m_model.fullView().at(slot).frontProcess;
  if (frontLayer && (*frontLayer == proc.id()))
  {
    on_layerModelPutToFront(slot, proc);
  }
  else
  {
    on_layerModelPutToBack(slot, proc);
  }

  updateProcessShape(slot, m_slots[slot].processes.back());
}

void FullViewConstraintPresenter::updateProcessShape(int slot, const LayerData& data)
{
  data.presenter->setHeight(m_model.fullView().at(slot).height - SlotHandle::handleHeight());

  auto width = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
  data.presenter->setWidth(width);
  data.presenter->parentGeometryChanged();
}

void FullViewConstraintPresenter::removeLayer(const Process::ProcessModel& proc)
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

void FullViewConstraintPresenter::on_slotRemoved(int pos)
{
  SlotPresenter& slot = m_slots.at(pos);
  for(LayerData& elt : slot.processes)
  {
    QPointer<Process::LayerView> view_p{elt.view};
    delete elt.presenter;
    if (view_p)
      deleteGraphicsItem(elt.view);
  }

  deleteGraphicsItem(slot.handle);
  //deleteGraphicsItem(slot.view);

  m_slots.erase(m_slots.begin() + pos);
}

void FullViewConstraintPresenter::updateProcessesShape()
{
  for(int i = 0; i < m_slots.size(); i++)
  {
    for(const LayerData& proc : m_slots[i].processes)
    {
      updateProcessShape(i, proc);
    }
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
    const Slot& model = m_model.fullView()[i];

    for(const LayerData& proc : slot.processes)
    {
      proc.view->setPos(0, currentSlotY);
      proc.view->update();
    }

    currentSlotY += model.height + slotSpacing; // Separation between slots
    slot.handle->setPos(0, currentSlotY - slotSpacing / 2);
  }

  // Horizontal shape
  on_defaultDurationChanged(m_model.duration.defaultDuration());

  updateProcessesShape();
}

double FullViewConstraintPresenter::rackHeight() const
{
  qreal height = 0;
  for(const auto& slot : m_model.fullView())
  {
    height += slot.height + slotSpacing;
  }
  return height;
}

void FullViewConstraintPresenter::on_layerModelPutToFront(int slot, const Process::ProcessModel& proc)
{
  // Put the selected one at z+1 and the others at -z; set "disabled" graphics
  // mode.
  // OPTIMIZEME by saving the previous to front and just switching...
  for (const LayerData& elt : m_slots.at(slot).processes)
  {
    if (elt.model->id() == proc.id())
    {
      elt.presenter->putToFront();
    }
    else
    {
      elt.presenter->putBehind();
    }
  }
}

void FullViewConstraintPresenter::on_layerModelPutToBack(int slot, const Process::ProcessModel& proc)
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
    for(const LayerData& proc : slot.processes)
    {
      proc.presenter->on_zoomRatioChanged(val);
    }
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
    for(const LayerData& proc : slot.processes)
    {
      proc.presenter->setWidth(w);
    }
  }
}

void FullViewConstraintPresenter::updateHeight()
{
  m_view->setHeight(rackHeight() + ConstraintHeader::headerHeight());

  updateChildren();
  emit heightChanged();
}
}
