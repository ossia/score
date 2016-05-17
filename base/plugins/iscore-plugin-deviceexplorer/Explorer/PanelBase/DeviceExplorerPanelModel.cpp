#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/PanelBase/DeviceExplorerPanelId.hpp>

#include "DeviceExplorerPanelModel.hpp"
#include <iscore/plugins/panel/PanelModel.hpp>

namespace Explorer
{
DeviceExplorerPanelModel::DeviceExplorerPanelModel(
        const iscore::DocumentContext& ctx,
        QObject* parent) :
    iscore::PanelModel {"DeviceExplorerPanelModel", parent},
    m_model {new DeviceExplorerModel{
                ctx.plugin<DeviceDocumentPlugin>(),
                this}}
{
}

int DeviceExplorerPanelModel::panelId() const
{
    return DEVICEEXPLORER_PANEL_ID;
}
}
