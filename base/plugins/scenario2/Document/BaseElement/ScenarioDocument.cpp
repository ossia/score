#include "ScenarioDocument.hpp"
#include "BaseElementModel.hpp"
#include "BaseElementPresenter.hpp"
#include "BaseElementView.hpp"

iscore::DocumentDelegateViewInterface*ScenarioDocument::makeView()
{
	return new BaseElementView;
}

iscore::DocumentDelegatePresenterInterface*ScenarioDocument::makePresenter(iscore::DocumentPresenter* parent_presenter,
																		   iscore::DocumentDelegateModelInterface* model,
																		   iscore::DocumentDelegateViewInterface* view)
{
	return new BaseElementPresenter{parent_presenter, model, view};
}

iscore::DocumentDelegateModelInterface* ScenarioDocument::makeModel()
{
	return new BaseElementModel{nullptr};
}