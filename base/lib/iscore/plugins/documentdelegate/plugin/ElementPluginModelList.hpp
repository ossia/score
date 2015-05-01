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

            // Can add if no exisiting element plugins with this id.
            bool canAdd(int pluginId) const;
            void add(iscore::ElementPluginModel* data);

            const QList<iscore::ElementPluginModel*>& list() const
            { return m_list; }

        signals:
            void pluginMetaDataChanged();

        private:
            QList<iscore::ElementPluginModel*> m_list;
    };
}
