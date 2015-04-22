#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

namespace iscore
{
    class Document;
    class ElementPluginModelList : public QObject
    {
            Q_OBJECT
        public:
            ElementPluginModelList(ElementPluginModelList* source, QObject* parent);
            ElementPluginModelList(iscore::Document *doc, QObject* parent);

            template<typename DeserializerVisitor>
            ElementPluginModelList(DeserializerVisitor&& vis, QObject* parent) :
                QObject{parent}
            {
                vis.writeTo(*this);
            }

            QString parentObjectName() const;

            bool canAdd(const QString& name) const;
            void add(iscore::ElementPluginModel* data);

            const auto& list() const { return m_list; }

        signals:
            void pluginMetaDataChanged();

        private:
            QList<iscore::ElementPluginModel*> m_list;
    };
}
