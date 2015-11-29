#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>

#include "DeviceExplorerPanelModel.hpp"
#include "DeviceExplorerPanelPresenter.hpp"
#include "DeviceExplorerPanelView.hpp"
#include <core/document/Document.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/panel/PanelModel.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include <Explorer/PanelBase/DeviceExplorerPanelId.hpp>

namespace iscore {
class PanelView;
class Presenter;
}  // namespace iscore

DeviceExplorerPanelPresenter::DeviceExplorerPanelPresenter(iscore::Presenter* parent,
                                                           iscore::PanelView* view) :
    iscore::PanelPresenter {parent, view}
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



