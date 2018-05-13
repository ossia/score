// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentPlugin.hpp"

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::DocumentPlugin)
W_OBJECT_IMPL(score::SerializableDocumentPlugin)
namespace score
{
class Document;

DocumentPlugin::DocumentPlugin(
    const score::DocumentContext& ctx,
    Id<DocumentPlugin> id,
    const QString& name,
    QObject* parent)
    : IdentifiedObject<DocumentPlugin>{id, name, parent}, m_context{ctx}
{
}

void DocumentPlugin::on_documentClosing()
{
}

DocumentPlugin::~DocumentPlugin() = default;
SerializableDocumentPlugin::~SerializableDocumentPlugin() = default;
DocumentPluginFactory::~DocumentPluginFactory() = default;

DocumentPluginFactoryList::~DocumentPluginFactoryList()
{
}

DocumentPluginFactoryList::object_type* DocumentPluginFactoryList::loadMissing(
    const VisitorVariant& vis, DocumentContext& doc, QObject* parent) const
{
  SCORE_TODO;
  return nullptr;
}

void SerializableDocumentPlugin::serializeAfterDocument(
    const VisitorVariant& vis) const
{
}

void SerializableDocumentPlugin::reloadAfterDocument(const VisitorVariant& vis)
{
}
}

template <>
void DataStreamReader::read(const score::SerializableDocumentPlugin& dpm)
{
}

template <>
void JSONObjectReader::read(const score::SerializableDocumentPlugin& dpm)
{
}
