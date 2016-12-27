#include "DocumentPlugin.hpp"

class QObject;
namespace iscore
{
class Document;

DocumentPlugin::DocumentPlugin(
    const iscore::DocumentContext& ctx,
    Id<DocumentPlugin>
        id,
    const QString& name,
    QObject* parent)
    : IdentifiedObject<DocumentPlugin>{id, name, parent}, m_context{ctx}
{
}

DocumentPlugin::~DocumentPlugin() = default;
SerializableDocumentPlugin::~SerializableDocumentPlugin() = default;
DocumentPluginFactory::~DocumentPluginFactory() = default;

DocumentPluginFactoryList::object_type* DocumentPluginFactoryList::loadMissing(
    const VisitorVariant& vis, DocumentContext& doc, QObject* parent) const
{
  ISCORE_TODO;
  return nullptr;
}

void SerializableDocumentPlugin::serializeAfterDocument(const VisitorVariant& vis) const
{
}

void SerializableDocumentPlugin::reloadAfterDocument(const VisitorVariant& vis)
{
}

}

template <>
void DataStreamReader::read(
    const iscore::SerializableDocumentPlugin& dpm)
{
}

template <>
void JSONObjectReader::readFromConcrete(
    const iscore::SerializableDocumentPlugin& dpm)
{
  readFrom(static_cast<const IdentifiedObject<iscore::DocumentPlugin>&>(dpm));
}
