#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <QJsonObject>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>

namespace iscore
{
// TODO : make it take a DocumentModel necessarily as parent.
class DocumentDelegatePluginModel : public NamedObject
{
        Q_OBJECT
    public:
        using NamedObject::NamedObject;
        virtual ~DocumentDelegatePluginModel() = default;

        virtual int elementPluginId() const = 0;
        virtual ElementPluginModel* makeElementPlugin(
                const QObject* element,
                QObject* parent) = 0;
        virtual ElementPluginModel* loadElementPlugin(
                const QObject* element,
                const VisitorVariant&,
                QObject* parent) = 0;

        virtual ElementPluginModel* cloneElementPlugin(
                const QObject* element,
                iscore::ElementPluginModel* source,
                QObject* parent) = 0;

        virtual QWidget* makeElementPluginWidget(
                const ElementPluginModel*,
                QWidget* parent) const = 0;

        virtual void serialize(const VisitorVariant&) const = 0;
};

class DocumentDelegatePluginModelFactory
{
    public:
        virtual DocumentDelegatePluginModel* make(QVariant data, QObject* parent) = 0;
};
}
