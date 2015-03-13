#pragma once

#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/command/OngoingCommandManager.hpp>

class NetworkControl;
class ClientSession;
class MasterSession;
namespace iscore
{
    class Document;
}
class NetworkDocumentClientPlugin : public iscore::DocumentDelegatePluginModel
{
        Q_OBJECT
    public:
        NetworkDocumentClientPlugin(ClientSession* s, NetworkControl* control, iscore::Document* doc);


    private:
        ClientSession* m_session{};
        NetworkControl* m_control{};
        iscore::Document* m_document{};
};



class NetworkDocumentMasterPlugin : public iscore::DocumentDelegatePluginModel
{
        Q_OBJECT
    public:
        NetworkDocumentMasterPlugin(MasterSession* s, NetworkControl* control, iscore::Document* doc);

    private:
        MasterSession* m_session{};
        NetworkControl* m_control{};
        iscore::Document* m_document{};
};
