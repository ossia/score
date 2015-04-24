#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/command/OngoingCommandManager.hpp>

#include "DistributedScenario/Group.hpp"

class NetworkControl;
class ClientSession;
class MasterSession;
class Session;
class GroupManager;

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
                const iscore::ElementPluginModel*, QWidget* widg) const override;

        void serialize(const VisitorVariant&) const override
        {
            qDebug() << Q_FUNC_INFO << "TODO";
        }


        GroupManager* groupManager() const
        { return m_groups; }

        auto policy() const { return m_policy; }

    private:
        void setupGroupPlugin(GroupMetadata* grp);

        NetworkPluginPolicy* m_policy;
        GroupManager* m_groups;

};

