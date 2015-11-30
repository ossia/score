#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <QObject>
#include <vector>

#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

class DataStream;
class GroupManager;
class GroupMetadata;
class JSONObject;
class QWidget;
class Session;
struct VisitorVariant;

namespace iscore
{
    class Document;
}

class iscore_plugin_networkPolicy : public QObject
{
    public:
        using QObject::QObject;
        virtual Session* session() const = 0;
};

class NetworkDocumentPlugin : public iscore::DocumentDelegatePluginModel
{
        Q_OBJECT

        ISCORE_SERIALIZE_FRIENDS(NetworkDocumentPlugin, DataStream)
        ISCORE_SERIALIZE_FRIENDS(NetworkDocumentPlugin, JSONObject)
    public:
        NetworkDocumentPlugin(iscore_plugin_networkPolicy* policy, iscore::Document& doc);

        // Loading has to be in two steps since the plugin policy is different from the client
        // and server.
        NetworkDocumentPlugin(const VisitorVariant& loader, iscore::Document& doc);
        void setPolicy(iscore_plugin_networkPolicy*);

        std::vector<iscore::ElementPluginModelType> elementPlugins() const override;

        iscore::ElementPluginModel* makeElementPlugin(
                const QObject* element,
                iscore::ElementPluginModelType,
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

        iscore_plugin_networkPolicy* policy() const
        { return m_policy; }

    signals:
        void sessionChanged();

    private:
        void setupGroupPlugin(GroupMetadata* grp);

        iscore_plugin_networkPolicy* m_policy{};
        GroupManager* m_groups{};

};

