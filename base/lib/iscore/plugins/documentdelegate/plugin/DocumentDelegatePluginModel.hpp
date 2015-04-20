#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <QJsonObject>

namespace iscore
{
    class DocumentDelegatePluginModel : public NamedObject
    {
            Q_OBJECT
        public:
            using NamedObject::NamedObject;
            virtual ~DocumentDelegatePluginModel() = default;

            virtual bool canMakeMetadata(const QString&) { return false; }
            virtual QVariant makeMetadata(const QString&) { return QVariant{}; }

            virtual QJsonObject toJson() { return QJsonObject{}; }
            virtual QByteArray toByteArray() { return QByteArray{}; }
    };
}
