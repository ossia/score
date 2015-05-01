#include "DeviceExplorerPanelPresenter.hpp"

#include "DeviceExplorerPanelView.hpp"
#include "DeviceExplorerPanelModel.hpp"

#include "Panel/DeviceExplorerModel.hpp"
#include "Panel/DeviceExplorerWidget.hpp"

#include "DeviceExplorerPanelId.hpp"

#include <core/document/DocumentModel.hpp>

DeviceExplorerPanelPresenter::DeviceExplorerPanelPresenter(iscore::Presenter* parent,
                                                           iscore::PanelViewInterface* view) :
    iscore::PanelPresenterInterface {parent, view}
{

}

void DeviceExplorerPanelPresenter::on_modelChanged()
{
    auto v = static_cast<DeviceExplorerPanelView*>(view());
    if(model())
    {
        auto m = static_cast<DeviceExplorerPanelModel *>(model());
        auto doc = iscore::IDocument::documentFromObject(model());
        m->m_model->setCommandQueue(&doc->commandStack());
        v->m_widget->setModel(m->m_model);
    }
    else
    {
        v->m_widget->setModel(nullptr);
    }
}

int DeviceExplorerPanelPresenter::panelId() const
{
    return DEVICEEXPLORER_PANEL_ID;
}



