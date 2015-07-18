#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

namespace iscore
{
    class Document;
    // The owner of the elements is the class that contains the ElementPluginModelList.
    class ElementPluginModelList
    {
        public:
            ElementPluginModelList() = default; // For safe serialization.
            // Note: we could instead call the deserialization ctor in the Element ctor...

            ElementPluginModelList(const ElementPluginModelList& source, QObject* parent);
            ElementPluginModelList(iscore::Document *doc, QObject* parent);
            ~ElementPluginModelList();

            QObject* parent();

            template<typename DeserializerType,
                     typename = std::enable_if_t<
                         not std::is_same<
                             typename std::decay<DeserializerType>::type,
                             iscore::ElementPluginModelList
                         >::value
                         and
                         not std::is_same<
                             typename std::decay<DeserializerType>::type,
                             iscore::ElementPluginModelList*
                         >::value>>
            ElementPluginModelList(DeserializerType&& vis, QObject* parent):
                m_parent{parent}
            {
                vis.writeTo(*this);
            }

            // Can add if no exisiting element plugins with this id.
            bool canAdd(ElementPluginModelType pluginId) const;
            void add(iscore::ElementPluginModel* data);

            const QList<iscore::ElementPluginModel*>& list() const
            { return m_list; }

        private:
            QObject* m_parent{};
            QList<iscore::ElementPluginModel*> m_list;
    };
}
