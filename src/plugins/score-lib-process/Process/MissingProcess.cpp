#include "MissingProcess.hpp"

#include <Process/Dataflow/PortFactory.hpp>

#include <score/application/ApplicationComponents.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/StringFactoryKeySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <QDebug>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Process::MissingProcess)

namespace Process
{
// Marks a DataStream tail as one WE wrote (see MissingProcess.hpp): a tail
// produced by the real process has a completely different layout, and reading
// it as ours would fabricate a bogus uuid.
static const constexpr quint32 missing_process_marker = 0x4D50524Fu; // 'MPRO'

namespace
{
//! Try to read one JSON value as a port. deserialize_interface hands back
//! nullptr when the value's uuid is not a port factory (PortFactoryList::
//! loadMissing returns nullptr), so anything that is not a port is simply
//! skipped -- curve segments, nested objects, plain data.
void try_load_port(
    const rapidjson::Value& v, const Process::PortFactoryList& pl,
    Process::Inlets& ins, Process::Outlets& outs, QObject* parent)
{
  if(!v.IsObject() || !v.HasMember("uuid"))
    return;

  JSONWriter wr{v};
  Process::Port* port{};
  try
  {
    port = deserialize_interface(pl, wr, parent);
  }
  catch(...)
  {
    return;
  }
  if(!port)
    return;

  if(auto* in = qobject_cast<Process::Inlet*>(port))
    ins.push_back(in);
  else if(auto* out = qobject_cast<Process::Outlet*>(port))
    outs.push_back(out);
  else
    port->deleteLater(); // ~Port is protected; Qt owns it through `parent`
}

//! Recreate the process's ports from its stored payload, WITHOUT knowing which
//! process it was. There is no single convention to key off: most processes
//! write "Inlets"/"Outlets" arrays (Process::readPorts), but plenty write single
//! named members instead -- Mapping writes "Inlet" and "Outlet". So every
//! top-level member is offered to the port factories, objects and arrays alike,
//! and whatever they accept becomes a port.
//!
//! The ports have to be REAL objects parented to the process, because that is
//! what a cable's Path<Process::Port> resolves against; without them every cable
//! touching this process is dropped.
void restore_ports(
    const rapidjson::Value& payload, const Process::PortFactoryList& pl,
    Process::Inlets& ins, Process::Outlets& outs, QObject* parent)
{
  if(!payload.IsObject())
    return;
  for(auto it = payload.MemberBegin(); it != payload.MemberEnd(); ++it)
  {
    const std::string_view k{it->name.GetString(), it->name.GetStringLength()};
    if(Process::MissingProcess::isBaseKey(k))
      continue;

    if(it->value.IsArray())
    {
      for(const auto& el : it->value.GetArray())
        try_load_port(el, pl, ins, outs, parent);
    }
    else
    {
      try_load_port(it->value, pl, ins, outs, parent);
    }
  }
}
}

MissingProcess::MissingProcess(JSONObject::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  // Identity. The process must be written back under the uuid it came in with,
  // so that a build which DOES have the factory loads it normally.
  QString uuidText;
  try
  {
    const auto u = vis.obj[vis.strings.uuid];
    uuidText = u.toString();
    JSONWriter wr{u};
    TSerializer<JSONObject, UuidKey<Process::ProcessModel>>::writeTo(wr, m_key);
  }
  catch(...)
  {
  }

  // Everything the concrete process wrote, kept verbatim for the next save.
  m_json = jsonToByteArray(vis.base);

  // Ports as REAL objects: this is what lets cable restoration find its
  // endpoints, and it is the whole reason the cables used to disappear with the
  // process. Guarded because writePorts SCORE_ABORTs on a malformed array.
  restore_ports(
      vis.base, vis.components.interfaces<Process::PortFactoryList>(), m_inlets,
      m_outlets, this);

  qWarning() << "This document uses a process this build does not have ("
             << uuidText << ", named" << metadata().getName()
             << "). It is kept as-is, with its ports and cables, and will be "
                "written back unchanged.";
}

MissingProcess::MissingProcess(DataStream::Deserializer& vis, QObject* parent)
    : Process::ProcessModel{vis, parent}
{
  quint32 marker{};
  vis.m_stream >> marker;
  if(marker != missing_process_marker)
  {
    // Not a tail we wrote: this is the real process's own binary layout and we
    // cannot recover its uuid from it (deserialize_interface has already
    // consumed the abstract key by the time loadMissing is reached). Leave
    // m_key null; ProcessFactoryList::loadMissing checks it and gives up rather
    // than inventing an identity.
    return;
  }

  TSerializer<DataStream, UuidKey<Process::ProcessModel>>::writeTo(vis, m_key);
  vis.m_stream >> m_json;

  if(!m_json.isEmpty())
  {
    const rapidjson::Document doc = readJson(m_json);
    if(doc.IsObject())
    {
      restore_ports(
          doc, vis.components.interfaces<Process::PortFactoryList>(), m_inlets,
          m_outlets, this);
    }
  }
  vis.checkDelimiter();
}

MissingProcess::~MissingProcess() { }

QString MissingProcess::prettyShortName() const noexcept
{
  return QObject::tr("Missing");
}

QString MissingProcess::category() const noexcept
{
  return QObject::tr("Other");
}

QStringList MissingProcess::tags() const noexcept
{
  return {};
}

ProcessFlags MissingProcess::flags() const noexcept
{
  return ProcessFlags{};
}

bool MissingProcess::isBaseKey(std::string_view k) noexcept
{
  // Written by JSONReader::readFromAbstract (uuid), by
  // TSerializer<JSONObject, score::Entity<ProcessModel>> (id, ObjectName,
  // Metadata, Components) and by JSONReader::read(const ProcessModel&).
  // Re-emitting them from the stored payload would duplicate the key in the
  // output object. Everything else -- the ports included, whatever shape they
  // are in -- is written back byte-for-byte, so the document a build WITH the
  // plug-in reopens is the one it saved.
  return k == "uuid" || k == "id" || k == "ObjectName" || k == "Metadata"
         || k == "Components" || k == "Duration" || k == "Height"
         || k == "StartOffset" || k == "LoopDuration" || k == "Pos" || k == "Size"
         || k == "Loops" || k == "FoldMode";
}

void MissingProcess::serialize_impl(const VisitorVariant& vis) const noexcept
{
  score::serialize_dyn(vis, *this);
}
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONReader::read(const Process::MissingProcess& proc)
{
  if(proc.m_json.isEmpty())
    return;

  const rapidjson::Document doc = readJson(proc.m_json);
  if(!doc.IsObject())
    return;

  for(auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it)
  {
    const std::string_view k{it->name.GetString(), it->name.GetStringLength()};
    if(Process::MissingProcess::isBaseKey(k))
      continue;
    stream.Key(it->name.GetString(), it->name.GetStringLength());
    it->value.Accept(stream);
  }
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::MissingProcess& proc)
{
  // Never reached: MissingProcess is built by its deserializing constructor,
  // which is the only way it can carry the original uuid.
}

template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::MissingProcess& proc)
{
  m_stream << Process::missing_process_marker;
  TSerializer<DataStream, UuidKey<Process::ProcessModel>>::readFrom(*this, proc.m_key);

  JSONReader r;
  r.stream.StartObject();
  r.read(proc);
  r.stream.EndObject();
  m_stream << r.toByteArray();

  insertDelimiter();
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::MissingProcess& proc)
{
  // Never reached: see the JSONWriter specialisation.
}
