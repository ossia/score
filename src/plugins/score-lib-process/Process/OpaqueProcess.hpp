#pragma once
#include <Process/Dataflow/Port.hpp>

#include <score/serialization/OpaquePayload.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>

#include <score_lib_process_export.h>

namespace Process
{
/**
 * @brief Stands in for a process whose factory this build does not have.
 *
 * Processes are provided by plug-ins that are not the same everywhere: VST and
 * LV2 do not exist in the wasm build, JIT needs x86_64, and several are
 * compiled in conditionally even on desktop. Before this existed, such a
 * process could not be loaded at all, and a document containing one was either
 * refused outright or -- worse -- opened with the process quietly dropped and
 * written back out without it.
 *
 * The point of this class is that a document must survive a round-trip through
 * a machine that cannot understand all of it: open on the machine that lacks
 * the plug-in, edit something else, save, reopen where the plug-in exists, and
 * find the process intact.
 *
 * So it keeps two things the plain ProcessModel base cannot:
 *
 *  - the concrete key of the process it replaces, returned from concreteKey().
 *    Forging that identity is the whole trick: saving writes the original UUID,
 *    so the file still names the real process rather than this placeholder.
 *  - the plug-in's own serialized data, verbatim, re-emitted untouched.
 *
 * Ports are an exception to "verbatim": they are pulled out of the payload and
 * rebuilt as real ports, so cables still connect and controls still hold and
 * report values. That is only possible in JSON, where they are stored under
 * known keys; the binary format writes them at a process-specific offset with
 * nothing to locate them by, so a binary payload is kept whole and the process
 * has no ports.
 */
class SCORE_LIB_PROCESS_EXPORT OpaqueProcessModel final : public ProcessModel
{
  W_OBJECT(OpaqueProcessModel)
  SCORE_SERIALIZE_FRIENDS

public:
  OpaqueProcessModel(
      const UuidKey<ProcessModel>& key, DataStream::Deserializer& vis, QObject* parent);
  OpaqueProcessModel(
      const UuidKey<ProcessModel>& key, JSONObject::Deserializer& vis, QObject* parent);

  //! For a command that asks to create a process whose factory we do not have.
  //!
  //! Unlike the loading constructors there is nothing to keep: the object was
  //! never serialized here, so this stand-in has no state and knows it. What
  //! the process should contain exists on the peer that could make it, and
  //! until that arrives this is a named placeholder -- see incomplete().
  OpaqueProcessModel(
      const UuidKey<ProcessModel>& key, const TimeVal& duration,
      const Id<ProcessModel>& id, QObject* parent);

  ~OpaqueProcessModel() override;

  //! The key of the process we replace, not one of our own.
  UuidKey<ProcessModel> concreteKey() const noexcept override { return m_key; }
  void serialize_impl(const VisitorVariant& vis) const noexcept override;

  QString prettyShortName() const noexcept override;
  QString category() const noexcept override;
  QStringList tags() const noexcept override;
  ProcessFlags flags() const noexcept override;

  //! Uuid of the absent process, for telling the user what is missing.
  QString missingFactory() const noexcept;

  //! True when the payload could not be split, so the ports are inside it and
  //! this process has none of its own.
  bool portsAreOpaque() const noexcept { return m_portsInPayload; }

  //! True when this stands in for an object whose state was never received:
  //! it was created by a command rather than read from a document.
  //!
  //! Its emptiness is not authoritative, so writing it out would tell a machine
  //! that *has* the plug-in that the process is empty, when it is only unknown
  //! here. Whoever creates one of these is responsible for filling it in.
  bool incomplete() const noexcept { return m_incomplete; }
  void setPayload(score::OpaquePayload payload, bool portsInPayload);

  //! The names of the JSON members written by ProcessModel and its bases.
  //! Anything else in a serialized process belongs to its plug-in.
  static const QStringList& baseMemberNames() noexcept;

private:
  UuidKey<ProcessModel> m_key;

  // The plug-in's own data, and which format it was read in. Minus the ports
  // when those could be rebuilt.
  score::OpaquePayload m_payload;
  bool m_portsInPayload{true};
  bool m_incomplete{false};
};

/**
 * @brief Stands in for a port whose factory this build does not have.
 *
 * A process can be understood while its ports are not: VST and LV2 bring their
 * own control port types along with the process itself. Without this,
 * reconstructing such a process aborted -- writePorts had SCORE_ABORT as its
 * failure path -- and the process could not be kept at all.
 *
 * Like OpaqueProcessModel it reports the key of the port it replaces and holds
 * the plug-in's data verbatim, so the port survives a save from here. It also
 * keeps the port's id, which is what cables resolve against: a stand-in with a
 * different id would silently break every cable pointing at it.
 */
class SCORE_LIB_PROCESS_EXPORT OpaqueInlet final : public Inlet
{
  W_OBJECT(OpaqueInlet)
  SCORE_SERIALIZE_FRIENDS
public:
  OpaqueInlet(
      const UuidKey<Port>& key, DataStream::Deserializer& vis, QObject* parent);
  OpaqueInlet(
      const UuidKey<Port>& key, JSONObject::Deserializer& vis, QObject* parent);
  ~OpaqueInlet() override;

  UuidKey<Port> concreteKey() const noexcept override { return m_key; }
  void serialize_impl(const VisitorVariant& vis) const noexcept override;
  PortType type() const noexcept override { return PortType::Message; }

private:
  UuidKey<Port> m_key;
  score::OpaquePayload m_payload;
};

class SCORE_LIB_PROCESS_EXPORT OpaqueOutlet final : public Outlet
{
  W_OBJECT(OpaqueOutlet)
  SCORE_SERIALIZE_FRIENDS
public:
  OpaqueOutlet(
      const UuidKey<Port>& key, DataStream::Deserializer& vis, QObject* parent);
  OpaqueOutlet(
      const UuidKey<Port>& key, JSONObject::Deserializer& vis, QObject* parent);
  ~OpaqueOutlet() override;

  UuidKey<Port> concreteKey() const noexcept override { return m_key; }
  void serialize_impl(const VisitorVariant& vis) const noexcept override;
  PortType type() const noexcept override { return PortType::Message; }

private:
  UuidKey<Port> m_key;
  score::OpaquePayload m_payload;
};

//! The names of the JSON members written by Port and its bases.
SCORE_LIB_PROCESS_EXPORT const QStringList& portBaseMemberNames() noexcept;

/**
 * @brief The layer used for a process no factory claims.
 *
 * The interval presenters build header and footer delegates from the result of
 * findDefaultFactory without checking it, so an OpaqueProcessModel needs some
 * factory or displaying it crashes. LayerFactory's defaults already produce a
 * usable plain layer, so this only has to exist and declare itself a fallback.
 */
class SCORE_LIB_PROCESS_EXPORT OpaqueLayerFactory final : public LayerFactory
{
  SCORE_CONCRETE("64a5b1ba-9d1e-4ba6-b6f5-e6f0aa0d0f7a")

  bool matches(const UuidKey<Process::ProcessModel>&) const override { return false; }
  bool isFallback() const noexcept override { return true; }
};
}
