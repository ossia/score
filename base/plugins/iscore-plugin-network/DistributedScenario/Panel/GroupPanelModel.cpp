
#include "DocumentPlugins/NetworkDocumentPlugin.hpp"
#include "GroupPanelModel.hpp"
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/plugins/panel/PanelModel.hpp>
#include "GroupPanelId.hpp"
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>


namespace Network
{
GroupPanelModel::GroupPanelModel(
        const iscore::DocumentContext& ctx,
        QObject* parent):
    iscore::PanelModel{"GroupPanelModel", parent}
{
    con(ctx.document.model(), &iscore::DocumentModel::pluginModelsChanged,
        this, [&] () {
        // Reference to ctx is safe beacause it is non-copyable; there is
        // a single instance originating from a document and there cannot
        // be spurious temporaries that might go out of scope.
        scanPlugins(ctx);
    });
}

GroupManager* GroupPanelModel::manager() const
{ return m_currentManager; }

Session* GroupPanelModel::session() const
{ return m_currentSession; }


void GroupPanelModel::scanPlugins(const iscore::DocumentContext& ctx)
{
    m_currentManager = nullptr;
    for(auto& plug : ctx.pluginModels())
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
}
