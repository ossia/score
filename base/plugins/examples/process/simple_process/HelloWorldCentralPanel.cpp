#include "HelloWorldCentralPanel.hpp"
#include "panel_impl/HelloWorldCentralPanelModel.hpp"
#include "panel_impl/HelloWorldCentralPanelPresenter.hpp"
#include "panel_impl/HelloWorldCentralPanelView.hpp"

using namespace iscore;


DocumentPanelView* HelloWorldCentralPanel::makeView()
{
	return new HelloWorldCentralPanelView;
}

DocumentPanelPresenter* HelloWorldCentralPanel::makePresenter(DocumentPresenter* parent_presenter, 
													  DocumentPanelModel* model, 
													  DocumentPanelView* view)
{
	return new HelloWorldCentralPanelPresenter(parent_presenter, model, view);
}

DocumentPanelModel* HelloWorldCentralPanel::makeModel()
{
	return new HelloWorldCentralPanelModel;
}
