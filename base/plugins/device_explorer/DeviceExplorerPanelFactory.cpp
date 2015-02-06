#include "DeviceExplorerPanelFactory.hpp"
#include "Panel/MainWindow.hpp"
#include "Panel/DeviceExplorerModel.hpp"
#include "Panel/DeviceExplorerWidget.hpp"
#include "document/DocumentPresenter.hpp"
#include <core/model/Model.hpp>
#include <core/view/View.hpp>
using namespace iscore;

//@todo split this in multiple files.

DeviceExplorerPanelView::DeviceExplorerPanelView(View* parent):
	iscore::PanelViewInterface{parent},
	m_widget{new DeviceExplorerWidget{parent}}
{
}

QWidget* DeviceExplorerPanelView::getWidget()
{
	return m_widget;
}


DeviceExplorerPanelModel::DeviceExplorerPanelModel(Model* parent):
	iscore::PanelModelInterface{"DeviceExplorerPanelModel", parent},
	m_model{new DeviceExplorerModel{this}}
{
}


DeviceExplorerPanelPresenter::DeviceExplorerPanelPresenter(iscore::Presenter* parent,
														   iscore::PanelModelInterface* model,
														   iscore::PanelViewInterface* view):
	iscore::PanelPresenterInterface{parent, model, view}
{
	auto v = static_cast<DeviceExplorerPanelView*>(view);
	auto m = static_cast<DeviceExplorerPanelModel*>(model);

	m->m_model->setCommandQueue(parent->document()->presenter()->commandQueue());
	v->m_widget->setModel(m->m_model);
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

