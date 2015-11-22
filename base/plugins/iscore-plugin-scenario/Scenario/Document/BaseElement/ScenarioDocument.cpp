#include "ScenarioDocument.hpp"

#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
#include <Scenario/Document/BaseElement/BaseElementView.hpp>

#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>

#include <iscore/serialization/VisitorCommon.hpp>
iscore::DocumentDelegateViewInterface*
ScenarioDocument::makeView(
        const iscore::ApplicationContext& ctx,
        iscore::DocumentView* parent)
{
    return new BaseElementView {ctx, parent};
}

iscore::DocumentDelegatePresenterInterface*
ScenarioDocument::makePresenter(
        iscore::DocumentPresenter* parent_presenter,
        const iscore::DocumentDelegateModelInterface& model,
        iscore::DocumentDelegateViewInterface& view)
{
    return new BaseElementPresenter {parent_presenter, model, view};
}

iscore::DocumentDelegateModelInterface*
ScenarioDocument::makeModel(iscore::DocumentModel* parent)
{
    return new BaseElementModel {parent};
}

iscore::DocumentDelegateModelInterface* ScenarioDocument::loadModel(
        const VisitorVariant& vis,
        iscore::DocumentModel* parent)
{

    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new BaseElementModel{deserializer, parent};});
}
