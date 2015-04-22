#include "ScenarioDocument.hpp"

#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
#include "Document/BaseElement/BaseElementView.hpp"

#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>

iscore::DocumentDelegateViewInterface*
ScenarioDocument::makeView(iscore::DocumentView* parent)
{
    return new BaseElementView {parent};
}

iscore::DocumentDelegatePresenterInterface*
ScenarioDocument::makePresenter(iscore::DocumentPresenter* parent_presenter,
                                iscore::DocumentDelegateModelInterface* model,
                                iscore::DocumentDelegateViewInterface* view)
{
    return new BaseElementPresenter {parent_presenter, model, view};
}

iscore::DocumentDelegateModelInterface*
ScenarioDocument::makeModel(iscore::DocumentModel* parent)
{
    return new BaseElementModel {parent};
}

iscore::DocumentDelegateModelInterface* ScenarioDocument::makeModel(QVariant data, iscore::DocumentModel* parent)
{
    return new BaseElementModel {data, parent};
}
