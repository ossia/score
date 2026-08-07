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
  // Written by serialize_impl when the ports are ours rather than the
  // payload's; the payload is read last because it takes the rest of the blob.
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
  // No payload: the creation data a command carries is what a factory would
  // have been *given*, not what the process would have *written*, and storing
  // one as the other would hand a machine that has the plug-in a blob it would
  // misread. Empty and flagged is honest; filling it in is a separate step.
  m_portsInPayload = false;
  awaitingRemoteState().push_back(this);
}

void OpaqueProcessModel::setState(const rapidjson::Value& serialized)
{
  if(!serialized.IsObject())
    return;

  // Ports first, and only if they are not already there: applying a state twice
  // would duplicate them, and a stand-in can be filled in more than once if the
  // object it stands for is edited.
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
  // TimeIndependent is not a guess about what we are replacing so much as a
  // statement about ourselves: nothing here knows how to rescale a plug-in's
  // data, so the parent duration changing must not be taken to change it. Left
  // out, the interval rewrote a stand-in's duration on every resize while its
  // contents stayed as they were -- and the processes most often standing in
  // like this, VST and LV2, declare it themselves.
  //
  // SupportsTemporal so it can still be shown where it was. Not ControlSurface
  // or RequiresCustomData: those promise things we cannot do.
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
