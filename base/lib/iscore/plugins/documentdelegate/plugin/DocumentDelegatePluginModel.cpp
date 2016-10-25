#include "DocumentDelegatePluginModel.hpp"
#include <iscore/tools/NamedObject.hpp>

class QObject;
namespace iscore {
class Document;

DocumentPlugin::DocumentPlugin(
        const iscore::DocumentContext& ctx,
        Id<DocumentPlugin> id,
        const QString& name,
        QObject* parent):
    IdentifiedObject<DocumentPlugin>{id, name, parent},
    m_context{ctx}
{

}

DocumentPlugin::~DocumentPlugin() = default;
SerializableDocumentPlugin::~SerializableDocumentPlugin() = default;
DocumentPluginFactory::~DocumentPluginFactory() = default;

DocumentPluginFactoryList::object_type*DocumentPluginFactoryList::loadMissing(
        const VisitorVariant& vis,
        DocumentContext& doc,
        QObject* parent) const
{
    ISCORE_TODO;
    return nullptr;
}

}

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(
        const iscore::SerializableDocumentPlugin& dpm)
{
    readFrom(static_cast<const IdentifiedObject<iscore::DocumentPlugin>&>(dpm));
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(
        const iscore::SerializableDocumentPlugin& dpm)
{
    readFrom(static_cast<const IdentifiedObject<iscore::DocumentPlugin>&>(dpm));
}

