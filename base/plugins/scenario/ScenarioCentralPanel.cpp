#include "ScenarioCentralPanel.hpp"
#include "panel_impl/ScenarioCentralPanelModel.hpp"
#include "panel_impl/ScenarioCentralPanelPresenter.hpp"
#include "panel_impl/ScenarioCentralPanelView.hpp"

using namespace iscore;


DocumentPanelView* ScenarioCentralPanel::makeView()
{
	return new ScenarioCentralPanelView;
}

DocumentPanelPresenter* ScenarioCentralPanel::makePresenter(DocumentPresenter* parent_presenter, 
													  DocumentPanelModel* model, 
													  DocumentPanelView* view)
{
	return new ScenarioCentralPanelPresenter(parent_presenter, model, view);
}

DocumentPanelModel* ScenarioCentralPanel::makeModel()
{
	return new ScenarioCentralPanelModel;
}
