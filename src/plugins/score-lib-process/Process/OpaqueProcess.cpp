#include "OpaqueProcess.hpp"

#include <Process/Dataflow/PortFactory.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QIODevice>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Process::OpaqueProcessModel)
W_OBJECT_IMPL(Process::OpaqueInlet)
W_OBJECT_IMPL(Process::OpaqueOutlet)

namespace Process
{
namespace
{
const QStringList& portMemberNames() noexcept
{
  static const QStringList names{QStringLiteral("Inlets"), QStringLiteral("Outlets")};
  return names;
}

//! Copy every member of `base` except those in `skip`, as a JSON object.
QByteArray captureMembers(const rapidjson::Value& base, const QStringList& skip)
{
  if(!base.IsObject())
    return {};

  rapidjson::StringBuffer buf;
  JsonWriter w{buf};
  w.StartObject();
  for(const auto& m : base.GetObject())
  {
    const auto name = QString::fromUtf8(m.name.GetString(), m.name.GetStringLength());
    if(skip.contains(name))
      continue;
    w.Key(m.name.GetString(), m.name.GetStringLength());
    m.value.Accept(w);
  }
  w.EndObject();

  if(buf.GetLength() <= 2)
    return {};
  return QByteArray{buf.GetString(), (int)buf.GetLength()};
}

void emitMembers(JsonWriter& stream, const QByteArray& blob)
{
  if(blob.isEmpty())
    return;

  rapidjson::Document d;
  d.Parse(blob.data(), blob.size());
  if(d.HasParseError() || !d.IsObject())
    return;

  for(const auto& m : d.GetObject())
  {
    stream.Key(m.name.GetString(), m.name.GetStringLength());
    m.value.Accept(stream);
  }
}
}

const QStringList& OpaqueProcessModel::baseMemberNames() noexcept
{
  // Written by readFromAbstract, IdentifiedObject, Entity and ProcessModel. A
  // serialized process holds these plus whatever its plug-in added; we own the
  // former and must not duplicate them, and must preserve the latter.
  // OpaqueProcessBaseMembersTest fails if this drifts.
  static const QStringList names{
      QStringLiteral("uuid"),       QStringLiteral("ObjectName"),
      QStringLiteral("id"),         QStringLiteral("Metadata"),
      QStringLiteral("Duration"),   QStringLiteral("Height"),
      QStringLiteral("StartOffset"), QStringLiteral("LoopDuration"),
      QStringLiteral("Pos"),        QStringLiteral("Size"),
      QStringLiteral("Loops"),      QStringLiteral("FoldMode")};
  return names;
}

OpaqueProcessModel::OpaqueProcessModel(
    const UuidKey<ProcessModel>& key, DataStream::Deserializer& vis, QObject* parent)
    : ProcessModel{vis, parent}
    , m_key{key}
{
  // The base consumed everything score itself writes, so the rest of this
  // object's blob is the plug-in's. deserialize_interface gave each polymorphic
  // object its own length-delimited buffer, which is what makes this safe: the
  // tail is exactly one process and stops where it should.
  if(auto* dev = vis.m_stream.stream.device())
    m_payload = dev->readAll();

  // No way to find the ports inside an opaque binary blob.
  m_portsInPayload = true;
}

OpaqueProcessModel::OpaqueProcessModel(
    const UuidKey<ProcessModel>& key, JSONObject::Deserializer& vis, QObject* parent)
    : ProcessModel{vis, parent}
    , m_key{key}
{
  auto skip = baseMemberNames();

  // Rebuild the ports when the plug-in stored them the usual way, so that
  // cables to this process still resolve and its controls still hold values.
  const bool hasPorts = vis.base.IsObject() && vis.base.HasMember("Inlets")
                        && vis.base.HasMember("Outlets");
  if(hasPorts)
  {
    auto& pl = score::AppContext().interfaces<Process::PortFactoryList>();
    writePorts(vis, pl, m_inlets, m_outlets, this);
    m_portsInPayload = false;
    skip += portMemberNames();
  }

  m_payload = captureMembers(vis.base, skip);
}

OpaqueProcessModel::~OpaqueProcessModel() = default;

void OpaqueProcessModel::serialize_impl(const VisitorVariant& vis) const noexcept
{
  if(vis.identifier == DataStream::type())
  {
    auto& s = static_cast<DataStream::Serializer&>(vis.visitor);
    s.m_stream.stream.writeRawData(m_payload.constData(), m_payload.size());
  }
  else if(vis.identifier == JSONObject::type())
  {
    auto& s = static_cast<JSONObject::Serializer&>(vis.visitor);
    if(!m_portsInPayload)
      readPorts(s, m_inlets, m_outlets);
    emitMembers(s.stream, m_payload);
  }
}

QString OpaqueProcessModel::missingFactory() const noexcept
{
  return QString::fromUtf8(score::uuids::toByteArray(m_key.impl()));
}

QString OpaqueProcessModel::prettyShortName() const noexcept
{
  const auto& name = metadata().getName();
  return name.isEmpty() ? QObject::tr("Unavailable") : name;
}

QString OpaqueProcessModel::category() const noexcept
{
  return QObject::tr("Unavailable");
}

QStringList OpaqueProcessModel::tags() const noexcept
{
  return {};
}

ProcessFlags OpaqueProcessModel::flags() const noexcept
{
  // Deliberately not TimeIndependent / ControlSurface / anything that would
  // make the rest of score treat it as usable: it has no executor and its
  // structure must not be edited, since we cannot re-derive the plug-in data.
  return {};
}
}

namespace Process
{
const QStringList& portBaseMemberNames() noexcept
{
  // Written by readFromAbstract, IdentifiedObject and Port. The last four are
  // only emitted when set, which is fine: we copy what is there.
  static const QStringList names{
      QStringLiteral("uuid"),     QStringLiteral("ObjectName"),
      QStringLiteral("id"),       QStringLiteral("Hidden"),
      QStringLiteral("Custom"),   QStringLiteral("Exposed"),
      QStringLiteral("Description"), QStringLiteral("Address")};
  return names;
}

namespace
{
QByteArray capturePortTail(DataStream::Deserializer& vis)
{
  if(auto* dev = vis.m_stream.stream.device())
    return dev->readAll();
  return {};
}

void emitPortPayload(const VisitorVariant& vis, const QByteArray& payload)
{
  if(vis.identifier == DataStream::type())
  {
    auto& s = static_cast<DataStream::Serializer&>(vis.visitor);
    s.m_stream.stream.writeRawData(payload.constData(), payload.size());
  }
  else if(vis.identifier == JSONObject::type())
  {
    auto& s = static_cast<JSONObject::Serializer&>(vis.visitor);
    emitMembers(s.stream, payload);
  }
}
}

OpaqueInlet::OpaqueInlet(
    const UuidKey<Port>& key, DataStream::Deserializer& vis, QObject* parent)
    : Inlet{vis, parent}
    , m_key{key}
    , m_payload{capturePortTail(vis)}
{
}

OpaqueInlet::OpaqueInlet(
    const UuidKey<Port>& key, JSONObject::Deserializer& vis, QObject* parent)
    : Inlet{vis, parent}
    , m_key{key}
    , m_payload{captureMembers(vis.base, portBaseMemberNames())}
{
}

OpaqueInlet::~OpaqueInlet() = default;

void OpaqueInlet::serialize_impl(const VisitorVariant& vis) const noexcept
{
  emitPortPayload(vis, m_payload);
}

OpaqueOutlet::OpaqueOutlet(
    const UuidKey<Port>& key, DataStream::Deserializer& vis, QObject* parent)
    : Outlet{vis, parent}
    , m_key{key}
    , m_payload{capturePortTail(vis)}
{
}

OpaqueOutlet::OpaqueOutlet(
    const UuidKey<Port>& key, JSONObject::Deserializer& vis, QObject* parent)
    : Outlet{vis, parent}
    , m_key{key}
    , m_payload{captureMembers(vis.base, portBaseMemberNames())}
{
}

OpaqueOutlet::~OpaqueOutlet() = default;

void OpaqueOutlet::serialize_impl(const VisitorVariant& vis) const noexcept
{
  emitPortPayload(vis, m_payload);
}
}
