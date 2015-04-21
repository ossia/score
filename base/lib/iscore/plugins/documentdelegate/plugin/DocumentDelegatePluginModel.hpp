#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <QJsonObject>

namespace iscore
{
    class ElementPluginModel : public QObject
    {
        public:
            using QObject::QObject;

            virtual QString plugin() const = 0;
    };

    // TODO : make it take a DocumentModel necessarily as parent.
    class DocumentDelegatePluginModel : public NamedObject
    {
            Q_OBJECT
        public:
            using NamedObject::NamedObject;
            virtual ~DocumentDelegatePluginModel() = default;

            virtual QString metadataName() const = 0;
            virtual ElementPluginModel* makeMetadata(const QString&) const = 0;
            virtual QWidget* makeMetadataWidget(const ElementPluginModel*) const = 0;

            virtual QJsonObject toJson() const = 0;
            virtual QByteArray toByteArray() const = 0;
    };

    class DocumentDelegatePluginModelFactory
    {
        public:
            virtual DocumentDelegatePluginModel* make(QVariant data, QObject* parent) = 0;
    };
}
