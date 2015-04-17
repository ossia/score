#pragma once

#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/command/OngoingCommandManager.hpp>
#include "DistributedScenario/Group.hpp"

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

        bool canMakeMetadata(const QString &) override;
        QVariant makeMetadata(const QString &) override;

    private:
        ClientSession* m_session{};
        NetworkControl* m_control{};
        iscore::Document* m_document{};

        std::vector<Group*> m_groups;

};


