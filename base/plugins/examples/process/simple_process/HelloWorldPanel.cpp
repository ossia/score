#include "HelloWorldPanel.hpp"
#include "panel_impl/HelloWorldPanelModel.hpp"
#include "panel_impl/HelloWorldPanelPresenter.hpp"
#include "panel_impl/HelloWorldPanelView.hpp"

using namespace iscore;


PanelView* HelloWorldPanel::makeView()
{
	return new HelloWorldPanelView;
}

PanelPresenter* HelloWorldPanel::makePresenter(Presenter* parent_presenter, 
											   PanelModel* model, 
											   PanelView* view)
{
	return new HelloWorldPanelPresenter(parent_presenter, model, view);
}

iscore::PanelModel* HelloWorldPanel::makeModel()
{
	return new HelloWorldPanelModel;
}
