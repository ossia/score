#include "OpaqueProcess.hpp"

#include <Process/RemoteState.hpp>

#include <Process/Dataflow/PortFactory.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/serialization/OpaquePayload.hpp>
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

}

const QStringList& OpaqueProcessModel::baseMemberNames() noexcept
{
  // What score itself writes; everything else in the blob is the plug-in's.
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
  // Ports when serialize_impl wrote them out; the payload takes the rest.
  bool portsWritten{};
  vis.m_stream >> portsWritten;
  if(portsWritten)
  {
    auto& pl = score::AppContext().interfaces<Process::PortFactoryList>();
    writePorts(vis, pl, m_inlets, m_outlets, this);
  }
  m_portsInPayload = !portsWritten;

  m_payload = score::OpaquePayload::fromDataStream(vis);
}

OpaqueProcessModel::OpaqueProcessModel(
    const UuidKey<ProcessModel>& key, const TimeVal& duration,
    const Id<ProcessModel>& id, QObject* parent)
    : ProcessModel{duration, id, QStringLiteral("OpaqueProcess"), parent}
    , m_key{key}
    , m_incomplete{true}
{
  // No payload: creation data is not what the process would have written.
  m_portsInPayload = false;
  awaitingRemoteState().push_back(this);
}

void OpaqueProcessModel::setState(const rapidjson::Value& serialized)
{
  if(!serialized.IsObject())
    return;

  // Only when absent: a stand-in can be filled in more than once.
  if(m_inlets.empty() && m_outlets.empty() && serialized.HasMember("Inlets")
     && serialized.HasMember("Outlets"))
  {
    JSONObject::Deserializer des{serialized};
    auto& pl = score::AppContext().interfaces<Process::PortFactoryList>();
    writePorts(des, pl, m_inlets, m_outlets, this);
    m_portsInPayload = false;
  }

  auto skip = baseMemberNames();
  if(!m_portsInPayload)
    skip += portMemberNames();

  m_payload = score::OpaquePayload::fromJson(serialized, skip);
  m_incomplete = false;
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

  m_payload = score::OpaquePayload::fromJson(vis.base, skip);
}

OpaqueProcessModel::~OpaqueProcessModel() = default;

void OpaqueProcessModel::serialize_impl(const VisitorVariant& vis) const noexcept
{
  // Live ports rather than the ones in the payload: they may have been edited,
  // and the payload no longer describes them.
  if(vis.identifier == JSONObject::type())
  {
    if(!m_portsInPayload)
      readPorts(static_cast<JSONObject::Serializer&>(vis.visitor), m_inlets, m_outlets);
  }
  else if(vis.identifier == DataStream::type())
  {
    auto& s = static_cast<DataStream::Serializer&>(vis.visitor);
    s.m_stream << !m_portsInPayload;
    if(!m_portsInPayload)
      readPorts(s, m_inlets, m_outlets);
  }

  m_payload.write(vis);
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
  // TimeIndependent: nothing here can rescale a plug-in's data.
  return ProcessFlags::SupportsTemporal | ProcessFlags::TimeIndependent;
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
}

OpaqueInlet::OpaqueInlet(
    const UuidKey<Port>& key, DataStream::Deserializer& vis, QObject* parent)
    : Inlet{vis, parent}
    , m_key{key}
    , m_payload{score::OpaquePayload::fromDataStream(vis)}
{
}

OpaqueInlet::OpaqueInlet(
    const UuidKey<Port>& key, JSONObject::Deserializer& vis, QObject* parent)
    : Inlet{vis, parent}
    , m_key{key}
    , m_payload{score::OpaquePayload::fromJson(vis.base, portBaseMemberNames())}
{
}

OpaqueInlet::~OpaqueInlet() = default;

void OpaqueInlet::serialize_impl(const VisitorVariant& vis) const noexcept
{
  m_payload.write(vis);
}

OpaqueOutlet::OpaqueOutlet(
    const UuidKey<Port>& key, DataStream::Deserializer& vis, QObject* parent)
    : Outlet{vis, parent}
    , m_key{key}
    , m_payload{score::OpaquePayload::fromDataStream(vis)}
{
}

OpaqueOutlet::OpaqueOutlet(
    const UuidKey<Port>& key, JSONObject::Deserializer& vis, QObject* parent)
    : Outlet{vis, parent}
    , m_key{key}
    , m_payload{score::OpaquePayload::fromJson(vis.base, portBaseMemberNames())}
{
}

OpaqueOutlet::~OpaqueOutlet() = default;

void OpaqueOutlet::serialize_impl(const VisitorVariant& vis) const noexcept
{
  m_payload.write(vis);
}
}
