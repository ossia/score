#include "NetworkPanel.hpp"
#include "panel_impl/NetworkPanelModel.hpp"
#include "panel_impl/NetworkPanelPresenter.hpp"
#include "panel_impl/NetworkPanelView.hpp"

using namespace iscore;


PanelView* NetworkPanel::makeView()
{
	return new NetworkPanelView;
}

PanelPresenter* NetworkPanel::makePresenter(Presenter* parent_presenter, 
											   PanelModel* model, 
											   PanelView* view)
{
	return new NetworkPanelPresenter(parent_presenter, model, view);
}

iscore::PanelModel* NetworkPanel::makeModel()
{
	return new NetworkPanelModel;
}
