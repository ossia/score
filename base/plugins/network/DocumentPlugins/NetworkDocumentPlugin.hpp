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

        QString metadataName() const override
        {
            return GroupMetadata::staticPluginName();
        }

        iscore::ElementPluginModel* makeElementPlugin(
                const QString & str,
                QObject* parent) override;

        iscore::ElementPluginModel* makeElementPlugin(
                const QString&,
                const VisitorVariant&,
                QObject* parent) override;

        iscore::ElementPluginModel* cloneElementPlugin(
                iscore::ElementPluginModel*,
                QObject* parent) override;

        virtual QWidget *makeElementPluginWidget(
                const iscore::ElementPluginModel*) const override;

        virtual QJsonObject toJson() const override;
        virtual QByteArray toByteArray() const override;



        GroupManager* groupManager() const
        { return m_groups; }

        auto policy() const { return m_policy; }

    private:
        NetworkPluginPolicy* m_policy;
        GroupManager* m_groups;

};

