#include "ScenarioCentralPanel.hpp"
#include "panel_impl/ScenarioCentralPanelModel.hpp"
#include "panel_impl/ScenarioCentralPanelPresenter.hpp"
#include "panel_impl/ScenarioCentralPanelView.hpp"

using namespace iscore;


PanelView* ScenarioCentralPanel::makeView()
{
	return new ScenarioCentralPanelView;
}

PanelPresenter* ScenarioCentralPanel::makePresenter(Presenter* parent_presenter, 
													  PanelModel* model, 
													  PanelView* view)
{
	return new ScenarioCentralPanelPresenter(parent_presenter, model, view);
}

iscore::PanelModel* ScenarioCentralPanel::makeModel()
{
	return new ScenarioCentralPanelModel;
}
