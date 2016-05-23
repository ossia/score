#include "DocumentDelegatePluginModel.hpp"
#include <iscore/tools/NamedObject.hpp>

class QObject;
namespace iscore {
class Document;

DocumentPlugin::DocumentPlugin(
        const iscore::DocumentContext& ctx,
        const QString& name,
        QObject* parent):
    NamedObject{name, parent},
    m_context{ctx}
{

}

DocumentPlugin::~DocumentPlugin() = default;
SerializableDocumentPlugin::~SerializableDocumentPlugin() = default;
DocumentPluginFactory::~DocumentPluginFactory() = default;

}

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(
        const iscore::SerializableDocumentPlugin& dpm)
{
    // Nothing to save
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(
        const iscore::SerializableDocumentPlugin& dpm)
{
    // Nothing to save
}

