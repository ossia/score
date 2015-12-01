#pragma once
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <type_traits>
#include <vector>

class QObject;
namespace iscore
{
    struct DocumentContext;

    // The owner of the elements is the class that contains the ElementPluginModelList.
    class ElementPluginModelList
    {
        public:
            ElementPluginModelList() = default; // For safe serialization.
            // Note: we could instead call the deserialization ctor in the Element ctor...

            ElementPluginModelList(const ElementPluginModelList& source, QObject* parent);
            ElementPluginModelList(
                    const iscore::DocumentContext& doc,
                    QObject* parent);
            ~ElementPluginModelList();

            ElementPluginModelList& operator=(ElementPluginModelList&& other);

            QObject* parent();

            template<typename DeserializerType,
                     enable_if_deserializer<DeserializerType>* = nullptr>
            ElementPluginModelList(DeserializerType&& vis, QObject* parent):
                m_parent{parent}
            {
                vis.writeTo(*this);
            }

            // Can add if no exisiting element plugins with this id.
            bool canAdd(ElementPluginModelType pluginId) const;
            void add(iscore::ElementPluginModel* data);

            const std::vector<iscore::ElementPluginModel*>& list() const
            { return m_list; }

        private:
            QObject* m_parent{};
            std::vector<iscore::ElementPluginModel*> m_list;
    };
}
