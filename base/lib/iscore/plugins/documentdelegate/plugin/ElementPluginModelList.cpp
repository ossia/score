#include "ElementPluginModelList.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

iscore::ElementPluginModelList::ElementPluginModelList(ElementPluginModelList* source,
                                                       QObject *parent):
    QObject{parent}
{
    // Used for cloning.
    iscore::Document* doc = iscore::IDocument::documentFromObject(source);
    for(ElementPluginModel* elt : source->m_list)
    {
        for(DocumentDelegatePluginModel* plugin : doc->model()->pluginModels())
        {
            if(plugin->metadataName() == elt->plugin())
            {
                add(plugin->cloneElementPlugin(parent, elt, this)); // Note : QObject::parent() is dangerous
            }
        }
    }
}

iscore::ElementPluginModelList::ElementPluginModelList(iscore::Document* doc, QObject *parent):
    QObject{parent}
{
    // We initialize the potential plug-ins of this document
    // with this object's metadata if necessary.

    for(auto& plugin : doc->model()->pluginModels())
    {
        // Check if it is not already existing in this element.
        if(!canAdd(plugin->metadataName()))
            continue;

        // Create and add it.
        auto plugElement = plugin->makeElementPlugin(parent, this);
        if(plugElement)
            add(plugElement);
    }
}

QString iscore::ElementPluginModelList::parentObjectName() const
{
    return parent()->metaObject()->className();
}

bool iscore::ElementPluginModelList::canAdd(const QString &name) const
{
    using namespace std;
    return none_of(begin(m_list), end(m_list),
                   [&] (iscore::ElementPluginModel* p)
    { return p->plugin() == name; });
}

void iscore::ElementPluginModelList::add(iscore::ElementPluginModel *data)
{
    Q_ASSERT(data);
    m_list.push_back(data);
    emit pluginMetaDataChanged();
}
