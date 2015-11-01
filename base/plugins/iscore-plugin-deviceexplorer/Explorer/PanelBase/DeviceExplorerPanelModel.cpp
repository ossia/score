#include "DeviceExplorerPanelModel.hpp"

#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include "DeviceExplorerPanelId.hpp"
#include <core/document/DocumentModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

DeviceExplorerPanelModel::DeviceExplorerPanelModel(iscore::DocumentModel* parent) :
    iscore::PanelModel {"DeviceExplorerPanelModel", parent},
    m_model {new DeviceExplorerModel{
                parent->findChild<DeviceDocumentPlugin*>("DeviceDocumentPlugin"),
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
