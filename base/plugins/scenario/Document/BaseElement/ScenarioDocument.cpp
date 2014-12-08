#include "ScenarioDocument.hpp"
#include "BaseElementModel.hpp"
#include "BaseElementPresenter.hpp"
#include "BaseElementView.hpp"

#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>

iscore::DocumentDelegateViewInterface*
ScenarioDocument::makeView(iscore::DocumentView* parent)
{
	return new BaseElementView{parent};
}

iscore::DocumentDelegatePresenterInterface*
ScenarioDocument::makePresenter(iscore::DocumentPresenter* parent_presenter,
								iscore::DocumentDelegateModelInterface* model,
								iscore::DocumentDelegateViewInterface* view)
{
	return new BaseElementPresenter{parent_presenter, model, view};
}

iscore::DocumentDelegateModelInterface*
ScenarioDocument::makeModel(iscore::DocumentModel* parent)
{
	return new BaseElementModel{parent};
}
