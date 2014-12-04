#include "HelloWorldPanel.hpp"
#include "panel_impl/HelloWorldPanelModel.hpp"
#include "panel_impl/HelloWorldPanelPresenter.hpp"
#include "panel_impl/HelloWorldPanelView.hpp"

using namespace iscore;


PanelViewInterface* HelloWorldPanel::makeView()
{
	return new HelloWorldPanelView;
}

PanelPresenterInterface* HelloWorldPanel::makePresenter(Presenter* parent_presenter, 
											   PanelModelInterface* model, 
											   PanelViewInterface* view)
{
	return new HelloWorldPanelPresenter(parent_presenter, model, view);
}

iscore::PanelModelInterface* HelloWorldPanel::makeModel()
{
	return new HelloWorldPanelModel;
}
