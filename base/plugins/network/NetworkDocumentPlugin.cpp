#include "NetworkDocumentPlugin.hpp"

#include <Repartition/session/MasterSession.hpp>
#include <Repartition/session/ClientSession.hpp>
#include <Repartition/client/RemoteClient.hpp>
#include <Repartition/session/ClientSessionBuilder.h>

#include "remote/RemoteActionReceiverMaster.hpp"
#include "remote/RemoteActionReceiverClient.hpp"


#include <core/document/Document.hpp>
#include <core/document/DocumentPresenter.hpp>
#include "NetworkCommand.hpp"
#include "settings_impl/NetworkSettingsModel.hpp"



NetworkDocumentClientPlugin::NetworkDocumentClientPlugin(ClientSession* s,
                                                         NetworkControl *control,
                                                         iscore::Document *doc):
    iscore::DocumentDelegatePluginModel{"NetworkDocumentPlugin", doc},
    m_session{s},
    m_control{control},
    m_document{doc}
{

}


NetworkDocumentMasterPlugin::NetworkDocumentMasterPlugin(MasterSession* s,
                                                         NetworkControl* control,
                                                         iscore::Document* doc):
    iscore::DocumentDelegatePluginModel{"NetworkDocumentPlugin", doc},
    m_session{s},
    m_control{control},
    m_document{doc}
{

}
