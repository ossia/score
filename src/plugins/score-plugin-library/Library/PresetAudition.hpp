#pragma once
#include <Process/Preset.hpp>
#include <Process/Process.hpp>

#include <QPointer>

#include <score_plugin_library_export.h>

#include <memory>

namespace score
{
class Command;
class Document;
}

namespace Library
{
/**
 * Presets loaded live into a process while the user moves through them in
 * the library: one undo step for the whole session, the last preset's.
 */
class SCORE_PLUGIN_LIBRARY_EXPORT PresetAudition final : public QObject
{
public:
  explicit PresetAudition(QObject* parent = nullptr);
  ~PresetAudition();

  //! Loading it only sets controls (ProcessModel::presetChangesPorts): one
  //! that can rebuild ports is not auditioned
  static bool canAudition(const Process::ProcessModel& proc, const Process::Preset& p);

  //! Loads it in place of the session's previous preset
  bool preview(const Process::ProcessModel& proc, const Process::Preset& p);
  //! Keeps the current preset, as one undo step
  void commit();
  //! Back to the state before the session
  void rollback();

  bool active() const noexcept { return bool(m_cmd); }
  const Process::ProcessModel* process() const noexcept { return m_proc; }

private:
  void end();

  QPointer<const Process::ProcessModel> m_proc;
  QPointer<score::Document> m_doc;
  std::unique_ptr<score::Command> m_cmd;
  //! The process's current preset before the session
  QStringList m_cursorBefore;
  QMetaObject::Connection m_pushed, m_gone;
};
}
