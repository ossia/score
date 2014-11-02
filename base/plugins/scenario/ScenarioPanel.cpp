#include "ScenarioPanel.hpp"
#include "panel_impl/ScenarioPanelModel.hpp"
#include "panel_impl/ScenarioPanelPresenter.hpp"
#include "panel_impl/ScenarioPanelView.hpp"

using namespace iscore;


PanelView* ScenarioPanel::makeView()
{
	return new ScenarioPanelView;
}

PanelPresenter* ScenarioPanel::makePresenter(Presenter* parent_presenter, 
											   PanelModel* model, 
											   PanelView* view)
{
	return new ScenarioPanelPresenter(parent_presenter, model, view);
}

iscore::PanelModel* ScenarioPanel::makeModel()
{
	return new ScenarioPanelModel;
}
