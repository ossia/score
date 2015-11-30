#include <core/document/DocumentModel.hpp>

#include "DocumentPlugins/NetworkDocumentPlugin.hpp"
#include "GroupPanelModel.hpp"
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/plugins/panel/PanelModel.hpp>
#include "GroupPanelId.hpp"

GroupPanelModel::GroupPanelModel(iscore::DocumentModel *model):
    iscore::PanelModel{"GroupPanelModel", model}
{
    connect(model, &iscore::DocumentModel::pluginModelsChanged,
            this, &GroupPanelModel::scanPlugins);
    scanPlugins();
}

GroupManager* GroupPanelModel::manager() const
{ return m_currentManager; }

Session* GroupPanelModel::session() const
{ return m_currentSession; }


void GroupPanelModel::scanPlugins()
{
    m_currentManager = nullptr;
    auto pluginModels = static_cast<iscore::DocumentModel*>(parent())->pluginModels();
    for(auto& plug : pluginModels)
    {
        if(auto netplug = dynamic_cast<NetworkDocumentPlugin*>(plug))
        {
            if(!netplug->policy())
                continue;


            connect(netplug, &NetworkDocumentPlugin::sessionChanged,
                    this, [=] () {
                m_currentManager = netplug->groupManager();
                m_currentSession = netplug->policy()->session();

                emit update();
            });

            m_currentManager = netplug->groupManager();
            m_currentSession = netplug->policy()->session();

            emit update();
            break;
        }
    }
}


int GroupPanelModel::panelId() const
{
    return GROUP_PANEL_ID;
}
