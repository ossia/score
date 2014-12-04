#include "HelloWorldCentralPanel.hpp"
#include "panel_impl/HelloWorldCentralPanelModel.hpp"
#include "panel_impl/HelloWorldCentralPanelPresenter.hpp"
#include "panel_impl/HelloWorldCentralPanelView.hpp"

using namespace iscore;


DocumentDelegateViewInterface* HelloWorldCentralPanel::makeView()
{
	return new HelloWorldCentralPanelView;
}

DocumentDelegatePresenterInterface* HelloWorldCentralPanel::makePresenter(DocumentPresenter* parent_presenter, 
													  DocumentDelegateModelInterface* model, 
													  DocumentDelegateViewInterface* view)
{
	return new HelloWorldCentralPanelPresenter(parent_presenter, model, view);
}

DocumentDelegateModelInterface* HelloWorldCentralPanel::makeModel()
{
	return new HelloWorldCentralPanelModel;
}
