#include <Process/ProcessContext.hpp>
#include <QGraphicsScene>
#include <QList>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
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
    const FullViewConstraintViewModel& cstr_model,
    const Process::ProcessPresenterContext& ctx,
    QGraphicsItem* parentobject,
    QObject* parent)
    : ConstraintPresenter{
          cstr_model, new FullViewConstraintView{*this, parentobject},
          new FullViewConstraintHeader{parentobject}, ctx, parent}
{
  // Update the address bar
  auto addressBar = static_cast<FullViewConstraintHeader*>(m_header)->bar();
  addressBar->setTargetObject(
      iscore::IDocument::unsafe_path(cstr_model.model()));
  connect(
      addressBar, &AddressBarItem::constraintSelected, this,
      &FullViewConstraintPresenter::constraintSelected);

  const auto& metadata = m_viewModel.model().metadata();
  con(metadata, &iscore::ModelMetadata::NameChanged, m_header,
      &ConstraintHeader::setText);
  m_header->setText(metadata.getName());
  m_header->show();

  if(rack())
  {
    for(const SlotPresenter& slot : rack()->getSlots())
    {
      for(const auto& layer : slot.processes())
      {
        Process::LayerView* view = layer.view;
        view->setFlag(QGraphicsItem::ItemClipsChildrenToShape, false);
      }
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
}
