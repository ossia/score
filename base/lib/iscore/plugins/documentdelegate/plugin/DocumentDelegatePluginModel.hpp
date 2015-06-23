#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <QJsonObject>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>

namespace iscore
{
class DocumentDelegatePluginModel : public NamedObject
{
        Q_OBJECT
    public:
        using NamedObject::NamedObject;
        virtual ~DocumentDelegatePluginModel() = default;

        virtual QList<int> elementPlugins() const { return {}; }
        virtual ElementPluginModel* makeElementPlugin(
                const QObject* element,
                QObject* parent) { return nullptr; }
        virtual ElementPluginModel* loadElementPlugin(
                const QObject* element,
                const VisitorVariant&,
                QObject* parent) { return nullptr; }

        virtual ElementPluginModel* cloneElementPlugin(
                const QObject* element,
                iscore::ElementPluginModel* source,
                QObject* parent) { return nullptr; }

        virtual QWidget* makeElementPluginWidget(
                const ElementPluginModel*,
                QWidget* parent) const { return nullptr; }

        virtual void serialize(const VisitorVariant&) const = 0;
};

class DocumentDelegatePluginModelFactory
{
    public:
        virtual DocumentDelegatePluginModel* make(QVariant data, QObject* parent) = 0;
};
}
