#include "DeviceExplorerPanelFactory.hpp"
#include <Panel/MainWindow.hpp>
#include <core/model/Model.hpp>
#include <core/view/View.hpp>
using namespace iscore;

DeviceExplorerPanelView::DeviceExplorerPanelView(View* parent):
	iscore::PanelViewInterface{parent}
{
	
}

QWidget* DeviceExplorerPanelView::getWidget()
{
	auto ptr = new MainWindow;
	
	return ptr;
}


iscore::PanelViewInterface*DeviceExplorerPanelFactory::makeView(iscore::View* parent)
{
	return new DeviceExplorerPanelView{parent};
}

iscore::PanelPresenterInterface*DeviceExplorerPanelFactory::makePresenter(
		iscore::Presenter* parent_presenter, 
		iscore::PanelModelInterface* model, 
		iscore::PanelViewInterface* view)
{
	return new DeviceExplorerPanelPresenter{parent_presenter, model, view};
}

iscore::PanelModelInterface*DeviceExplorerPanelFactory::makeModel(iscore::Model* parent)
{
	return new DeviceExplorerPanelModel{parent};
}

DeviceExplorerPanelModel::DeviceExplorerPanelModel(Model* parent): 
	iscore::PanelModelInterface{parent, "DeviceExplorerPanelModel"}
{ 
}


DeviceExplorerPanelPresenter::DeviceExplorerPanelPresenter(iscore::Presenter* parent, iscore::PanelModelInterface* model, iscore::PanelViewInterface* view):
	iscore::PanelPresenterInterface{parent, model, view}
{
	
}
