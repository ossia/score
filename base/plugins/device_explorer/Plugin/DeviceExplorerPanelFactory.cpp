#include "DeviceExplorerPanelFactory.hpp"
#include "Panel/DeviceExplorerModel.hpp"
#include "Panel/DeviceExplorerWidget.hpp"
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <Singletons/DeviceExplorerInterface.hpp>
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

DeviceExplorerPanelModel::DeviceExplorerPanelModel(const VisitorVariant& data, DocumentModel *parent):
    iscore::PanelModelInterface {"DeviceExplorerPanelModel", parent},
    m_model {new DeviceExplorerModel{data, this}}
{

}

void DeviceExplorerPanelModel::serialize(const VisitorVariant &vis) const
{
    if(vis.identifier == DataStream::type())
    {
        auto& ser = *static_cast<DataStream::Serializer*>(vis.visitor);
        ser.m_stream << (bool) m_model->rootNode();
        if(m_model->rootNode())
            ser.readFrom(*m_model->rootNode());
    }
    else if(vis.identifier == JSON::type())
    {
        auto& ser = *static_cast<JSON::Serializer*>(vis.visitor);
        if(m_model->rootNode())
            ser.readFrom(*m_model->rootNode());
    }
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

PanelModelInterface *DeviceExplorerPanelFactory::makeModel(const VisitorVariant& data, DocumentModel *parent)
{
    return new DeviceExplorerPanelModel {data, parent};
}
