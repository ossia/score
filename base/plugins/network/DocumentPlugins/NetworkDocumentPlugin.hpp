#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/command/OngoingCommandManager.hpp>

#include "DistributedScenario/Group.hpp"
#include <iscore/serialization/VisitorCommon.hpp>

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
        using QObject::QObject;
        virtual Session* session() const = 0;
};

class NetworkDocumentPlugin : public iscore::DocumentDelegatePluginModel
{
        Q_OBJECT
        friend void Visitor<Reader<DataStream>>::readFrom<NetworkDocumentPlugin>(const NetworkDocumentPlugin& ev);
        friend void Visitor<Reader<JSONObject>>::readFrom<NetworkDocumentPlugin>(const NetworkDocumentPlugin& ev);
        friend void Visitor<Writer<DataStream>>::writeTo<NetworkDocumentPlugin>(NetworkDocumentPlugin& ev);
        friend void Visitor<Writer<JSONObject>>::writeTo<NetworkDocumentPlugin>(NetworkDocumentPlugin& ev);
    public:
        NetworkDocumentPlugin(NetworkPluginPolicy* policy, iscore::DocumentModel *doc);

        // Loading has to be in two steps since the plugin policy is different from the client
        // and server.
        NetworkDocumentPlugin(const VisitorVariant& loader, iscore::DocumentModel* doc);
        void setPolicy(NetworkPluginPolicy*);

        QString metadataName() const override
        {
            return GroupMetadata::staticPluginName();
        }

        iscore::ElementPluginModel* makeElementPlugin(
                const QObject* element,
                QObject* parent) override;

        iscore::ElementPluginModel* loadElementPlugin(
                const QObject* element,
                const VisitorVariant&,
                QObject* parent) override;

        iscore::ElementPluginModel* cloneElementPlugin(
                const QObject* element,
                iscore::ElementPluginModel*,
                QObject* parent) override;

        virtual QWidget *makeElementPluginWidget(
                const iscore::ElementPluginModel*, QWidget* widg) const override;

        void serialize(const VisitorVariant&) const override;


        GroupManager* groupManager() const
        { return m_groups; }

        NetworkPluginPolicy* policy() const
        { return m_policy; }

    private:
        void setupGroupPlugin(GroupMetadata* grp);

        NetworkPluginPolicy* m_policy{};
        GroupManager* m_groups{};

};

