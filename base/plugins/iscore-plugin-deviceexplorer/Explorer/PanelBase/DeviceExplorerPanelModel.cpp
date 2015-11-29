#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include "Explorer/PanelBase/DeviceExplorerPanelId.hpp"
#include <core/document/DocumentModel.hpp>

#include "DeviceExplorerPanelModel.hpp"
#include <iscore/plugins/panel/PanelModel.hpp>

DeviceExplorerPanelModel::DeviceExplorerPanelModel(iscore::DocumentModel* parent) :
    iscore::PanelModel {"DeviceExplorerPanelModel", parent},
    m_model {new DeviceExplorerModel{
                *parent->findChild<DeviceDocumentPlugin*>("DeviceDocumentPlugin"),
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
