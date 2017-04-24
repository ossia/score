#include <Process/ProcessContext.hpp>
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
#include <Process/LayerView.hpp>
#include "AddressBarItem.hpp"
#include "FullViewConstraintHeader.hpp"
#include "FullViewConstraintPresenter.hpp"
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>
#include <Process/Focus/FocusDispatcher.hpp>

class QObject;

namespace Scenario
{
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

  createRackPresenter();

  for(const SlotPresenter& slot : rack().getSlots())
  {
    for(const auto& layer : slot.processes())
    {
      Process::LayerView* view = layer.view;
      view->setFlag(QGraphicsItem::ItemClipsChildrenToShape, false);
    }
  }
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

RackPresenter& FullViewConstraintPresenter::rack() const
{
  return *m_rack;
}


void FullViewConstraintPresenter::createRackPresenter()
{
  auto rackView = new RackView{m_view};
  rackView->setPos(0, ConstraintHeader::headerHeight());

  // Cas par défaut
  m_rack = new RackPresenter{
           m_model.fullViewRack(),
           rackView, m_context, this};

  m_rack->on_zoomRatioChanged(m_zoomRatio);

  connect(
        m_rack, &RackPresenter::askUpdate, this,
        &FullViewConstraintPresenter::updateHeight);

  connect(m_rack, &RackPresenter::pressed,
          this, &ConstraintPresenter::pressed);
  connect(m_rack, &RackPresenter::moved,
          this, &ConstraintPresenter::moved);
  connect(m_rack, &RackPresenter::released,
          this, &ConstraintPresenter::released);
}

void FullViewConstraintPresenter::updateScaling()
{
  ConstraintPresenter::updateScaling();
  updateHeight();
}

void FullViewConstraintPresenter::on_zoomRatioChanged(ZoomRatio val)
{
  ConstraintPresenter::on_zoomRatioChanged(val);

  m_rack->on_zoomRatioChanged(m_zoomRatio);
}

void FullViewConstraintPresenter::on_defaultDurationChanged(const TimeVal& v)
{
  ConstraintPresenter::on_defaultDurationChanged(v);

  m_rack->setWidth(m_view->defaultWidth());
}

void FullViewConstraintPresenter::updateHeight()
{
  view()->setHeight(m_rack->height() + ConstraintHeader::headerHeight());

  updateChildren();
  emit heightChanged();
}
}
