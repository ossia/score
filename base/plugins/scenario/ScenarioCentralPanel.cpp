#include "ScenarioCentralPanel.hpp"
#include "panel_impl/ScenarioCentralPanelModel.hpp"
#include "panel_impl/ScenarioCentralPanelPresenter.hpp"
#include "panel_impl/ScenarioCentralPanelView.hpp"

using namespace iscore;


DocumentDelegateViewInterface* ScenarioCentralPanel::makeView()
{
	return new ScenarioCentralPanelView;
}

DocumentDelegatePresenterInterface* ScenarioCentralPanel::makePresenter(DocumentPresenter* parent_presenter, 
													  DocumentDelegateModelInterface* model, 
													  DocumentDelegateViewInterface* view)
{
	return new ScenarioCentralPanelPresenter(parent_presenter, model, view);
}

DocumentDelegateModelInterface* ScenarioCentralPanel::makeModel()
{
	return new ScenarioCentralPanelModel;
}
