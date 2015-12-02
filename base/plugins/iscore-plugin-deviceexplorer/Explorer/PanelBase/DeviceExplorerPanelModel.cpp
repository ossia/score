#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/PanelBase/DeviceExplorerPanelId.hpp>

#include "DeviceExplorerPanelModel.hpp"
#include <iscore/plugins/panel/PanelModel.hpp>

DeviceExplorerPanelModel::DeviceExplorerPanelModel(
        const iscore::DocumentContext& ctx,
        QObject* parent) :
    iscore::PanelModel {"DeviceExplorerPanelModel", parent},
    m_model {new DeviceExplorerModel{
                ctx.plugin<DeviceDocumentPlugin>(),
                this}}
{
}

DeviceExplorerModel* DeviceExplorerPanelModel::deviceExplorer()
{
    return m_model;
}


int DeviceExplorerPanelModel::panelId() const
{
    return DEVICEEXPLORER_PANEL_ID;
}
