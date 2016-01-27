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
    readFrom(dpm.concreteFactoryKey().impl());
    dpm.serialize_impl(this->toVariant());
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const iscore::SerializableDocumentPlugin& dpm)
{
    m_obj["uuid"] = toJsonValue(dpm.concreteFactoryKey().impl());
    dpm.serialize_impl(this->toVariant());
}

