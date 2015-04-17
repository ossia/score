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

class NetworkDocumentMasterPlugin : public iscore::DocumentDelegatePluginModel
{
        Q_OBJECT
    public:
        NetworkDocumentMasterPlugin(MasterSession* s, NetworkControl* control, iscore::Document* doc);

        bool canMakeMetadata(const QString &) override;
        QVariant makeMetadata(const QString &) override;

        GroupManager* groupManager() const
        { return m_groups; }

        MasterSession* session() const
        { return m_session; }

    private:
        MasterSession* m_session{};
        NetworkControl* m_control{};
        iscore::Document* m_document{};

        GroupManager* m_groups;
};
