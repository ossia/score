#pragma once
#include <Recording/Engine/AudioRecorder.hpp>
#include <Recording/Engine/MidiRecorder.hpp>
#include <Recording/Engine/ValueRecorder.hpp>
#include <Recording/Session/RecordOptions.hpp>
#include <Recording/Takes/WavStreamWriter.hpp>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <score_plugin_recording_export.h>

#include <verdigris>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace score
{
struct DocumentContext;
}
namespace Process
{
class Port;
class ProcessModel;
}
namespace Scenario
{
class IntervalModel;
}
namespace Execution
{
class TapLink;
}

namespace Recording
{
/**
 * One recording, from the UI action that starts it to the undoable step that
 * places its takes.
 *
 * It lives on the GUI thread. The engine records and reports: a recorder node
 * per source, linked to the port it records, times its take on the box the
 * port plays in and hands what it records to a writer thread, which streams it
 * to the Takes folder. The session ends when the user stops it, when playback
 * stops, or when the takes reach the length asked; it then waits for the
 * files, unlinks the recorders, and places every take in one command. Undo and
 * redo are off while it runs.
 */
class SCORE_PLUGIN_RECORDING_EXPORT RecordSession final : public QObject
{
  W_OBJECT(RecordSession)
public:
  RecordSession(const score::DocumentContext& ctx, RecordOptions options);
  ~RecordSession() override;

  //! An audio, MIDI or value port; false for one that cannot be recorded
  bool addSource(const Process::Port& port);
  int sources() const noexcept { return int(m_sources.size()); }

  //! Starts the recording, or waits for playback (RecordOptions::Start)
  bool start();
  //! Ends the recording: what was recorded is placed
  void stop();
  bool running() const noexcept { return m_running; }

  //! The takes are placed (or nothing was recorded)
  void finished() E_SIGNAL(SCORE_PLUGIN_RECORDING_EXPORT, finished)

  //! What a take of the session became: its file, and where it starts in its
  //! box. Filled when the session ends.
  struct Take
  {
    QString file;
    int64_t startDate{};
    int64_t endDate{};
    bool midi{};
    bool values{};
    QPointer<Scenario::IntervalModel> box;
    QPointer<Process::ProcessModel> placed;
  };
  const std::vector<Take>& takes() const noexcept { return m_takes; }

private:
  struct Source;
  void createRecorders();
  void writerLoop();
  void drain();
  void poll();
  void finish();
  void place();

  const score::DocumentContext& m_ctx;
  RecordOptions m_options;
  std::vector<std::unique_ptr<Source>> m_sources;
  std::vector<Take> m_takes;
  QTimer m_poll;
  std::thread m_writer;
  std::atomic_bool m_writing{false};
  std::mutex m_drainLock;
  QMetaObject::Connection m_started, m_finished, m_undoGuard, m_redoGuard;
  bool m_running{false};
  bool m_recordersCreated{false};
};
}
