#include "ScenarioDocumentFactory.hpp"

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>

#include <iscore/serialization/VisitorCommon.hpp>
iscore::DocumentDelegateViewInterface*
ScenarioDocument::makeView(
        const iscore::ApplicationContext& ctx,
        iscore::DocumentView* parent)
{
    return new ScenarioDocumentView {ctx, parent};
}

iscore::DocumentDelegatePresenterInterface*
ScenarioDocument::makePresenter(
        iscore::DocumentPresenter* parent_presenter,
        const iscore::DocumentDelegateModelInterface& model,
        iscore::DocumentDelegateViewInterface& view)
{
    return new ScenarioDocumentPresenter {parent_presenter, model, view};
}

iscore::DocumentDelegateModelInterface*
ScenarioDocument::makeModel(iscore::DocumentModel* parent)
{
    return new ScenarioDocumentModel {parent};
}

iscore::DocumentDelegateModelInterface* ScenarioDocument::loadModel(
        const VisitorVariant& vis,
        iscore::DocumentModel* parent)
{

    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new ScenarioDocumentModel{deserializer, parent};});
}
