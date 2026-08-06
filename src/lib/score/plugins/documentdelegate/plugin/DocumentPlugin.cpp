// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentPlugin.hpp"

#include <score/serialization/OpaquePayload.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::DocumentPlugin)
W_OBJECT_IMPL(score::OpaqueDocumentPlugin)
W_OBJECT_IMPL(score::SerializableDocumentPlugin)
namespace score
{
class Document;

DocumentPlugin::DocumentPlugin(
    const score::DocumentContext& ctx, const QString& name, QObject* parent)
    : QObject{parent}
    , m_context{ctx}
{
  setObjectName(name);
}

void DocumentPlugin::on_documentClosing() { }

DocumentPlugin::~DocumentPlugin() = default;
SerializableDocumentPlugin::~SerializableDocumentPlugin() = default;
DocumentPluginFactory::~DocumentPluginFactory() = default;

DocumentPluginFactoryList::~DocumentPluginFactoryList() { }

DocumentPluginFactoryList::object_type* DocumentPluginFactoryList::loadMissing(
    const UuidKey<score::DocumentPluginFactory>& key, const VisitorVariant& vis,
    DocumentContext& doc, QObject* parent) const
{
  switch(vis.identifier)
  {
    case DataStream::type():
      return new OpaqueDocumentPlugin{
          key, doc, static_cast<DataStream::Deserializer&>(vis.visitor), parent};
    case JSONObject::type():
      return new OpaqueDocumentPlugin{
          key, doc, static_cast<JSONObject::Deserializer&>(vis.visitor), parent};
  }
  return nullptr;
}

OpaqueDocumentPlugin::OpaqueDocumentPlugin(
    const UuidKey<DocumentPluginFactory>& key, const score::DocumentContext& ctx,
    DataStream::Deserializer& vis, QObject* parent)
    : SerializableDocumentPlugin{ctx, vis, parent}
    , m_key{key}
    , m_payload{score::capturedTail(vis)}
{
}

OpaqueDocumentPlugin::OpaqueDocumentPlugin(
    const UuidKey<DocumentPluginFactory>& key, const score::DocumentContext& ctx,
    JSONObject::Deserializer& vis, QObject* parent)
    : SerializableDocumentPlugin{ctx, vis, parent}
    , m_key{key}
    // A document plug-in's base writes nothing but the key, so everything else
    // in the object belongs to the plug-in.
    , m_payload{score::capturedMembers(vis.base, {QStringLiteral("uuid")})}
{
}

OpaqueDocumentPlugin::~OpaqueDocumentPlugin() = default;

void OpaqueDocumentPlugin::serialize_impl(const VisitorVariant& vis) const noexcept
{
  if(vis.identifier == DataStream::type())
    score::writeCapturedTail(
        static_cast<DataStream::Serializer&>(vis.visitor), m_payload);
  else if(vis.identifier == JSONObject::type())
    score::writeCapturedMembers(
        static_cast<JSONObject::Serializer&>(vis.visitor).stream, m_payload);
}
}

template <>
void DataStreamReader::read(const score::SerializableDocumentPlugin& dpm)
{
}

template <>
void JSONReader::read(const score::SerializableDocumentPlugin& dpm)
{
}
