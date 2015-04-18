#include "GroupPanelModel.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/Document.hpp>

#include "DocumentPlugins/NetworkClientDocumentPlugin.hpp"
#include "DocumentPlugins/NetworkMasterDocumentPlugin.hpp"
#include "Repartition/session/ClientSession.hpp"
#include "Repartition/session/MasterSession.hpp"

GroupPanelModel::GroupPanelModel(iscore::DocumentModel *model):
    iscore::PanelModelInterface{"GroupPanelModel", model}
{
    connect(model, &iscore::DocumentModel::pluginModelsChanged,
            this, &GroupPanelModel::scanPlugins);
    scanPlugins();
}


void GroupPanelModel::scanPlugins()
{
    qDebug(Q_FUNC_INFO);
    m_currentManager = nullptr;
    auto pluginModels = static_cast<iscore::DocumentModel*>(parent())->pluginModels();
    for(auto& plug : pluginModels)
    {
        if(plug->objectName() == "NetworkDocumentClientPlugin")
        {
            auto netplug = static_cast<NetworkDocumentClientPlugin*>(plug);
            m_currentManager = netplug->groupManager();
            m_currentSession = netplug->session();

            emit update();
            break;
        }
        else if(plug->objectName() == "NetworkDocumentMasterPlugin")
        {
            qDebug("da");
            auto netplug = static_cast<NetworkDocumentMasterPlugin*>(plug);
            m_currentManager = netplug->groupManager();
            m_currentSession = netplug->session();

            emit update();
            break;
        }
    }
}
