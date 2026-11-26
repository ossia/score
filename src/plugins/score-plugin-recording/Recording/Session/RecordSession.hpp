#pragma once
#include <Recording/Engine/AudioRecorder.hpp>
#include <Recording/Engine/MidiRecorder.hpp>
#include <Recording/Engine/ValueRecorder.hpp>
#include <Recording/Session/RecordOptions.hpp>
#include <Recording/Takes/PlaceAddressTakes.hpp>
#include <Recording/Takes/WavStreamWriter.hpp>

#include <State/Address.hpp>

#include <Scenario/Palette/ScenarioPoint.hpp>

#include <nano_signal_slot.hpp>

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>

#include <score/model/Identifier.hpp>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <score_plugin_recording_export.h>

#include <verdigris>

#include <atomic>
#include <memory>
#include <optional>
#include <mutex>
#include <thread>
#include <vector>

namespace score
{
struct DocumentContext;
}
namespace Process
{
struct ProcessData;
class Port;
class ProcessModel;
}
namespace Scenario
{
class IntervalModel;
class ProcessModel;
namespace Command
{
class Macro;
}
}
namespace Device
{
class DeviceInterface;
}
namespace Execution
{
class TapLink;
}
namespace ossia
{
class time_interval;
class time_process;
class graph_node;
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
class SCORE_PLUGIN_RECORDING_EXPORT RecordSession final
    : public QObject
    , public Nano::Observer
{
  W_OBJECT(RecordSession)
public:
  RecordSession(const score::DocumentContext& ctx, RecordOptions options);
  ~RecordSession() override;

  //! An audio, MIDI or value port; false for one that cannot be recorded
  bool addSource(const Process::Port& port);
  //! An address of a device: what it receives is recorded, as an inlet bound
  //! to it would get it. Its takes are placed in a new box (placeInNewBox).
  bool addAddress(const State::Address& address);
  int sources() const noexcept { return int(m_sources.size()); }

  //! The takes of addresses go to a new box of `scenario`, at `point`: the
  //! clock is the box `scenario` plays in. With RecordOptions::Start::FirstEvent
  //! while stopped, the session waits for the first value any address
  //! receives, and then starts playback from `point` when `playOnFirst`.
  void placeInNewBox(
      const Scenario::ProcessModel& scenario, Scenario::Point point,
      AddressPlacement placement, CurveOptions curves, bool playOnFirst);
  //! The takes of addresses go to processes of `box`, which is also their
  //! clock: they start where they were recorded in it
  void placeInBox(
      Scenario::IntervalModel& box, AddressPlacement placement, CurveOptions curves);
  //! The box the takes of addresses were placed in
  Scenario::IntervalModel* placedBox() const noexcept;

  //! Starts the recording, or waits for playback (RecordOptions::Start)
  bool start();
  //! Why start() refused, for the user
  const QString& error() const noexcept { return m_error; }
  //! Ends the recording: what was recorded is placed
  void stop();
  //! Ends it before returning, its takes placed: for when its document goes
  //! away. Waits for the audio engine's next tick.
  void stopNow();
  //! RecordOptions::capture: ends it, placing the last bars played. Ended
  //! otherwise (playback stops), nothing is placed and its files are removed.
  void capture();
  bool running() const noexcept { return m_running; }
  const RecordOptions& options() const noexcept { return m_options; }
  //! Running, and no take records yet: playback, the grid or a first event is
  //! awaited
  bool waiting() const noexcept;

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
  //! What the user should know of the takes: a tempo that changed meanwhile,
  //! a speed other than 1. Filled when the session ends.
  const QStringList& warnings() const noexcept { return m_warnings; }
  //! Once playback ran: whether any source could be recorded at all
  bool linkedAny() const noexcept { return m_linkedAny; }

private:
  struct Source;
  //! `atStart`: playback is being set up, its clock not running yet
  void createRecorders(bool atStart = false);
  void writerLoop();
  void drain();
  void poll();
  void finish();
  //! Frames of audio a peak of the live view covers
  static constexpr int peakFrames = 256;
  void place();
  //! Where playback starts for a recording into a new box of a scenario
  TimeVal playDate() const;
  void lockUndo();
  void unlockUndo();
  void removeFiles();
  bool m_discard{};
  bool m_linkedAny{};
  //! A Sound take's tempo and stretch, said rather than guessed from its file
  void setTiming(
      Scenario::Command::Macro& macro, Process::ProcessModel& take, double tempo,
      bool loop);
  void warnAboutTiming();
  QStringList m_warnings;
  QString m_error;
  //! Bytes free on the disk of the Takes folder; -1 if unknown
  int64_t freeBytes() const;
  QElapsedTimer m_diskCheck;
  //! What the takes recorded so far, for the views of their boxes
  void showLiveTakes();
  void updateLiveTakes();
  void hideLiveTakes();
  int m_liveTick{};
  //! A process of a take, in a new slot, at its place in the node view
  Process::ProcessModel* createTake(
      Scenario::Command::Macro& macro, const Scenario::IntervalModel& box,
      const Process::ProcessData& data);
  //! A take of an inlet plays into it
  void connectToInlet(
      Scenario::Command::Macro& macro, const Process::ProcessModel& take,
      const Process::Port& port);
  //! `bars`: the passes' length; with `oneLoop`, the take is one loop that
  //! long whatever it lasted
  bool placePasses(
      Source& s, Take& t, std::optional<Scenario::Command::Macro>& macro, double bars,
      bool oneLoop);
  bool hasAddresses() const noexcept;
  //! The takes are cut in passes, or looped over a number of bars
  bool inPasses() const noexcept;
  //! Audio or texture inputs among the addresses: they wait for no message
  bool hasStreamAddresses() const noexcept;
  void listenToDevices();
  void stopListening();
  void deviceValue(const State::Address& addr, const ossia::value& v);
  void firstValue(const State::Address& addr, const ossia::value& v);

  const score::DocumentContext& m_ctx;
  RecordOptions m_options;
  std::vector<std::unique_ptr<Source>> m_sources;
  std::vector<Take> m_takes;
  QTimer m_poll;
  std::thread m_writer;
  std::atomic_bool m_writing{false};
  std::mutex m_drainLock;
  //! Where the writer thread puts its files
  QString m_folder;
  QMetaObject::Connection m_started, m_finished;
  bool m_undoLocked{};
  bool m_running{false};
  bool m_recordersCreated{false};

  // Takes of addresses
  QPointer<const Scenario::ProcessModel> m_scenario;
  QPointer<Scenario::IntervalModel> m_intoBox;
  Scenario::Point m_point{};
  AddressPlacement m_placement{AddressPlacement::Automations};
  CurveOptions m_curves{};
  bool m_playOnFirst{true};
  //! By id: the command that makes it may make it again
  std::optional<Id<Scenario::IntervalModel>> m_placedBox;
  // Waiting for the first value, while stopped
  std::vector<QPointer<Device::DeviceInterface>> m_listened;
  std::vector<State::Address> m_listeningAdded;
  bool m_firstReceived{false};

  // Recording into an automation: it is silent meanwhile
  std::shared_ptr<ossia::time_process> m_latched;
  //! Touch: values came less than the release ago
  void touch();
  bool m_touching{};

  // The count-in, while the takes wait
  std::shared_ptr<ossia::graph_node> m_countIn;
public:
  //! The beats the count-in clicked, while it counts in
  int countInBeats() const noexcept;

private:
  //! Every process the takes were placed as, passes included
  std::vector<QPointer<Process::ProcessModel>> m_placed;
  int m_countInBeats{};
  //! The boxes the takes are timed on, ticked while recording even empty
  std::vector<std::shared_ptr<ossia::time_interval>> m_observed;
  void removeCountIn();
};
}
