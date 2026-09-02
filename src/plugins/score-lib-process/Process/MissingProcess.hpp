#pragma once
#include <Process/Process.hpp>

#include <score_lib_process_export.h>

namespace Process
{
/**
 * @brief Placeholder for a process whose factory is not installed.
 *
 * Before this existed, `ProcessFactoryList::loadMissing` returned nullptr and
 * the caller (IntervalModelSerialization) did `SCORE_TODO` — so a document
 * saved with an add-on the current build does not have came back with the
 * process GONE, and, because its ports went with it, every cable attached to
 * it gone too. The document then reported as loaded successfully and, on the
 * next save, the loss became permanent. It is the only silent data-loss path in
 * the loader: every other failure is loud.
 *
 * MissingProcess keeps everything needed to give the document back unchanged:
 *  - the ORIGINAL uuid, returned from concreteKey(), so the process is written
 *    back under the identity it was saved with and a build that DOES have the
 *    factory loads it normally;
 *  - the real Inlet / Outlet objects, so `Dataflow` cable restoration finds its
 *    endpoints and the cables survive;
 *  - the rest of the concrete payload verbatim, re-emitted on save.
 *
 * It has no behaviour: it does not execute, and its layer is the default
 * "name in a box" one (LayerFactoryList::findDefaultFactory falls back to a
 * placeholder factory for an unknown key, since the scenario presenters
 * dereference that pointer unconditionally).
 */
class SCORE_LIB_PROCESS_EXPORT MissingProcess final : public Process::ProcessModel
{
  W_OBJECT(MissingProcess)
  SCORE_SERIALIZE_FRIENDS
public:
  MissingProcess(JSONObject::Deserializer& vis, QObject* parent);
  MissingProcess(DataStream::Deserializer& vis, QObject* parent);
  ~MissingProcess() override;

  QString prettyShortName() const noexcept override;
  QString category() const noexcept override;
  QStringList tags() const noexcept override;
  ProcessFlags flags() const noexcept override;

  //! The uuid of the process this stands in for. Written back as-is.
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override { return m_key; }
  void serialize_impl(const VisitorVariant& vis) const noexcept override;

  //! Keys that Entity<ProcessModel> / ProcessModel write themselves; re-emitting
  //! them from the stored payload would duplicate them in the output object.
  static bool isBaseKey(std::string_view k) noexcept;

  UuidKey<Process::ProcessModel> m_key{};

  //! The concrete process's own JSON, kept verbatim (empty for DataStream).
  QByteArray m_json;
  //! The concrete process's own DataStream tail, kept verbatim (empty for JSON).
  QByteArray m_binary;
};
}
