#include "GroupPanelModel.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/Document.hpp>

#include "DocumentPlugins/NetworkClientDocumentPlugin.hpp"
#include "DocumentPlugins/NetworkMasterDocumentPlugin.hpp"

GroupPanelModel::GroupPanelModel(iscore::DocumentModel *model):
    iscore::PanelModelInterface{"GroupPanelModel", model}
{
    connect(model, &iscore::DocumentModel::pluginModelsChanged,
            this, &GroupPanelModel::scanPlugins);
    scanPlugins();
}


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
