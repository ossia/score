#include "ElementPluginModelList.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

iscore::ElementPluginModelList::ElementPluginModelList(
        iscore::Document* doc,
        QObject *parent):
    m_parent{parent}
{
    // We initialize the potential plug-ins of this document
    // with this object's metadata if necessary.

    for(auto& plugin : doc->model()->pluginModels())
    {
        for(const auto& plugid : plugin->elementPlugins())
        {
            // Check if it is not already existing in this element.
            if(!canAdd(plugid))
                continue;

            // Create and add it.
            if(auto plugElement = plugin->makeElementPlugin(m_parent, plugid, m_parent))
                add(plugElement);
        }
    }
}

iscore::ElementPluginModelList::ElementPluginModelList(
        const ElementPluginModelList& source,
        QObject *parent):
    m_parent{parent}
{
    // Used for cloning.
    iscore::Document* doc = iscore::IDocument::documentFromObject(m_parent);
    for(ElementPluginModel* elt : source.m_list)
    {
        for(DocumentDelegatePluginModel* plugin : doc->model()->pluginModels())
        {
            if(plugin->elementPlugins().contains(elt->elementPluginId()))
            {
                add(plugin->cloneElementPlugin(m_parent, elt, m_parent)); // Note : QObject::parent() is dangerous
            }
        }
    }
}

iscore::ElementPluginModelList::~ElementPluginModelList()
{
    qDeleteAll(m_list);
}

QObject*iscore::ElementPluginModelList::parent()
{
    return m_parent;
}

bool iscore::ElementPluginModelList::canAdd(ElementPluginModelType pluginId) const
{
    using namespace std;
    return none_of(begin(m_list), end(m_list),
                   [&] (iscore::ElementPluginModel* p)
    { return p->elementPluginId() == pluginId; });
}

void iscore::ElementPluginModelList::add(iscore::ElementPluginModel *data)
{
    Q_ASSERT(data);
    m_list.push_back(data);
}
