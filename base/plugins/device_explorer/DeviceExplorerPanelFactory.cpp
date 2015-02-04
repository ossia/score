#include "DeviceExplorerPanelFactory.hpp"
#include "Panel/MainWindow.hpp"
#include "Panel/DeviceExplorerModel.hpp"
#include <core/model/Model.hpp>
#include <core/view/View.hpp>
using namespace iscore;

//@todo split this in multiple files.

DeviceExplorerPanelView::DeviceExplorerPanelView(View* parent, DeviceExplorerMainWindow* mw):
	iscore::PanelViewInterface{parent},
	m_deviceExplorer{mw}
{
	this->setObjectName(tr("Device explorer"));
}

QWidget* DeviceExplorerPanelView::getWidget()
{
	return m_deviceExplorer;
}


DeviceExplorerPanelFactory::DeviceExplorerPanelFactory():
	m_deviceExplorer{new DeviceExplorerMainWindow}

{

}

iscore::PanelViewInterface*DeviceExplorerPanelFactory::makeView(iscore::View* parent)
{
	return new DeviceExplorerPanelView{parent, m_deviceExplorer};
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
	m_deviceExplorer->model()->setParent(parent);
	return new DeviceExplorerPanelModel{parent};
}

DeviceExplorerPanelModel::DeviceExplorerPanelModel(Model* parent):
	iscore::PanelModelInterface{"DeviceExplorerPanelModel", parent}
{
}


DeviceExplorerPanelPresenter::DeviceExplorerPanelPresenter(iscore::Presenter* parent, iscore::PanelModelInterface* model, iscore::PanelViewInterface* view):
	iscore::PanelPresenterInterface{parent, model, view}
{

}
