#include "DocumentDelegatePluginModel.hpp"
#include <iscore/tools/NamedObject.hpp>

class QObject;
namespace iscore {
class Document;

DocumentPluginModel::DocumentPluginModel(
        iscore::Document& ctx,
        const QString& name,
        QObject* parent):
    NamedObject{name, parent},
    m_context{ctx}
{

}

DocumentPluginModel::~DocumentPluginModel()
{

}

SerializableDocumentPluginModel::~SerializableDocumentPluginModel()
{

}

DocumentPluginModelFactory::~DocumentPluginModelFactory()
{

}

}

template<>
void Visitor<Reader<DataStream>>::readFrom(
        const iscore::SerializableDocumentPluginModel& dpm)
{
    readFrom(dpm.concreteFactoryKey().impl());
    dpm.serialize_impl(this->toVariant());
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(
        const iscore::SerializableDocumentPluginModel& dpm)
{
    m_obj["uuid"] = toJsonValue(dpm.concreteFactoryKey().impl());
    dpm.serialize_impl(this->toVariant());
}

