#include "DeviceExplorerPanelFactory.hpp"
#include "Panel/DeviceExplorerModel.hpp"
#include "Panel/DeviceExplorerWidget.hpp"
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/view/View.hpp>
using namespace iscore;

//@todo split this in multiple files.

DeviceExplorerPanelView::DeviceExplorerPanelView(View* parent) :
    iscore::PanelViewInterface {parent},
m_widget {new DeviceExplorerWidget{parent}}
{
    setObjectName("Device Explorer");
}

QWidget* DeviceExplorerPanelView::getWidget()
{
    return m_widget;
}


DeviceExplorerPanelModel::DeviceExplorerPanelModel(DocumentModel* parent) :
    iscore::PanelModelInterface {"DeviceExplorerPanelModel", parent},
m_model {new DeviceExplorerModel{this}}
{
}


DeviceExplorerPanelPresenter::DeviceExplorerPanelPresenter(iscore::Presenter* parent,
        iscore::PanelViewInterface* view) :
    iscore::PanelPresenterInterface {parent, view}
{

}

void DeviceExplorerPanelPresenter::on_modelChanged()
{
    auto v = static_cast<DeviceExplorerPanelView*>(view());
    auto m = static_cast<DeviceExplorerPanelModel*>(model());

    // TODO make a function to get the document here
    auto doc = IDocument::documentFromObject(model());
    m->m_model->setCommandQueue(&doc->commandStack());
    v->m_widget->setModel(m->m_model);
}







iscore::PanelViewInterface* DeviceExplorerPanelFactory::makeView(iscore::View* parent)
{
    return new DeviceExplorerPanelView {parent};
}

iscore::PanelPresenterInterface* DeviceExplorerPanelFactory::makePresenter(iscore::Presenter* parent_presenter,
        iscore::PanelViewInterface* view)
{
    return new DeviceExplorerPanelPresenter {parent_presenter, view};
}

iscore::PanelModelInterface* DeviceExplorerPanelFactory::makeModel(DocumentModel* parent)
{
    return new DeviceExplorerPanelModel {parent};
}

