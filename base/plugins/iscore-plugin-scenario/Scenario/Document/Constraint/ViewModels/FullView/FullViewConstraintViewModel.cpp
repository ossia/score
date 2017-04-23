#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <iscore/tools/std/Optional.hpp>

#include "FullViewConstraintViewModel.hpp"
#include <Process/ZoomHelper.hpp>
#include <Process/LayerModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/model/Identifier.hpp>

#include <iscore/document/DocumentContext.hpp>
#include <core/document/DocumentPresenter.hpp>
class QObject;
namespace Scenario
{
FullViewConstraintViewModel::FullViewConstraintViewModel(
    const Id<ConstraintViewModel>& id, ConstraintModel& model, QObject* parent)
    : ConstraintViewModel{id, "FullViewConstraintViewModel", model, parent}
{
}

FullViewConstraintViewModel* FullViewConstraintViewModel::clone(
    const Id<ConstraintViewModel>& id, ConstraintModel& cm, QObject* parent)
{
  auto cstr = new FullViewConstraintViewModel{id, cm, parent};
  cstr->showRack(this->shownRack());

  return cstr;
}

ZoomRatio FullViewConstraintViewModel::zoom() const
{
  return m_zoom;
}

void FullViewConstraintViewModel::setZoom(const ZoomRatio& zoom)
{
  m_zoom = zoom;
}

QRectF FullViewConstraintViewModel::visibleRect() const
{
  return m_center;
}

void FullViewConstraintViewModel::setVisibleRect(const QRectF& value)
{
  m_center = value;
}

bool FullViewConstraintViewModel::isActive()
{
  auto& ctx = iscore::IDocument::documentContext(model());
  auto& baseElt = iscore::IDocument::get<ScenarioDocumentPresenter>(ctx.document);

  return (&this->model() == &baseElt.displayedElements.constraint());
}

bool isInFullView(const ConstraintModel& cstr)
{
  auto& doc = iscore::IDocument::documentContext(cstr);
  auto& sub = safe_cast<Scenario::ScenarioDocumentPresenter&>(
                doc.document.presenter().presenterDelegate());
  return &sub.displayedElements.constraint() == &cstr;
}

bool isInFullView(const Process::LayerModel& theLM)
{
  if(auto p = theLM.parent())
    if(auto p2 = p->parent())
      if(auto cst = qobject_cast<ConstraintModel*>(p2->parent()))
        return isInFullView(*cst);
  return false;
}

}
