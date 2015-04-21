#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <QJsonObject>

namespace iscore
{
    // TODO : make it take a DocumentModel necessarily as parent.
    class DocumentDelegatePluginModel : public NamedObject
    {
            Q_OBJECT
        public:
            using NamedObject::NamedObject;
            virtual ~DocumentDelegatePluginModel() = default;

            virtual bool canMakeMetadata(const QString&) const = 0;
            virtual QVariant makeMetadata(const QString&) const = 0;
            virtual bool canMakeMetadataWidget(const QVariant&) const = 0;
            virtual QWidget* makeMetadataWidget(const QVariant&) const = 0;

            virtual QJsonObject toJson() const = 0;
            virtual QByteArray toByteArray() const = 0;
    };

    class DocumentDelegatePluginModelFactory
    {
        public:
            virtual DocumentDelegatePluginModel* make(QVariant data, QObject* parent) = 0;
    };
}
