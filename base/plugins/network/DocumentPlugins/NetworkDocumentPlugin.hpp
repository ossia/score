#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/command/OngoingCommandManager.hpp>

#include "DistributedScenario/Group.hpp"

class NetworkControl;
class ClientSession;
class MasterSession;
class Session;
namespace iscore
{
    class Document;
}

class NetworkPluginPolicy : public QObject
{
    public:
        virtual Session* session() const = 0;
};

class NetworkDocumentPlugin : public iscore::DocumentDelegatePluginModel
{
    public:
        NetworkDocumentPlugin(NetworkPluginPolicy* policy, iscore::Document *doc);

        bool canMakeMetadata(const QString & str) const override;
        QVariant makeMetadata(const QString & str) const override;

        virtual bool canMakeMetadataWidget(const QVariant &) const override;
        virtual QWidget *makeMetadataWidget(const QVariant &) const override;
        virtual QJsonObject toJson() const override;
        virtual QByteArray toByteArray() const override;



        GroupManager* groupManager() const
        { return m_groups; }

        auto policy() const { return m_policy; }

    private:
        NetworkPluginPolicy* m_policy;
        GroupManager* m_groups;

};

