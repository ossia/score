#include "HelloWorldCentralPanel.hpp"
#include "panel_impl/HelloWorldCentralPanelModel.hpp"
#include "panel_impl/HelloWorldCentralPanelPresenter.hpp"
#include "panel_impl/HelloWorldCentralPanelView.hpp"

using namespace iscore;


PanelView* HelloWorldCentralPanel::makeView()
{
	return new HelloWorldCentralPanelView;
}

PanelPresenter* HelloWorldCentralPanel::makePresenter(Presenter* parent_presenter, 
													  PanelModel* model, 
													  PanelView* view)
{
	return new HelloWorldCentralPanelPresenter(parent_presenter, model, view);
}

iscore::PanelModel* HelloWorldCentralPanel::makeModel()
{
	return new HelloWorldCentralPanelModel;
}
