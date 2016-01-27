#include "DocumentDelegatePluginModel.hpp"
#include <iscore/tools/NamedObject.hpp>

class QObject;
namespace iscore {
class Document;

DocumentPlugin::DocumentPlugin(
        iscore::Document& ctx,
        const QString& name,
        QObject* parent):
    NamedObject{name, parent},
    m_context{ctx}
{

}

DocumentPlugin::~DocumentPlugin()
{

}

SerializableDocumentPlugin::~SerializableDocumentPlugin()
{

}

DocumentPluginFactory::~DocumentPluginFactory()
{

}

}

template<>
void Visitor<Reader<DataStream>>::readFrom(
        const iscore::SerializableDocumentPlugin& dpm)
{
    // Nothing to save
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const iscore::SerializableDocumentPlugin& dpm)
{
    // Nothing to save
}

