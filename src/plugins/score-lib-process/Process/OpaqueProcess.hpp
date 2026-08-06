#pragma once
#include <Process/Process.hpp>

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

  //! The names of the JSON members written by ProcessModel and its bases.
  //! Anything else in a serialized process belongs to its plug-in.
  static const QStringList& baseMemberNames() noexcept;

private:
  UuidKey<ProcessModel> m_key;

  // JSON: an object holding the plug-in's members, minus the ports when those
  // could be rebuilt. DataStream: the raw tail of this object's blob.
  QByteArray m_payload;
  bool m_portsInPayload{true};
};
}
