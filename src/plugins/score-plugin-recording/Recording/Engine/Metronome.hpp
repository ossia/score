#pragma once
#include <Process/ExecutionAction.hpp>
#include <Process/TimeValue.hpp>

#include <Media/Metro/MetroNode.hpp>

#include <ossia/dataflow/graph_node.hpp>

#include <QObject>

#include <score_plugin_recording_export.h>

#include <atomic>
#include <memory>
#include <vector>

namespace score
{
struct DocumentContext;
}
namespace ossia::net
{
class parameter_base;
}
namespace Scenario
{
class IntervalModel;
}
namespace Recording
{
class ApplicationPlugin;

/**
 * The count-in before playback: while it counts, the score waits and the
 * clicks are written to the channels of the cue output. Its end falls on the
 * end of a buffer, so that the score starts exactly one beat after the last
 * click: the first click comes as late into the first buffer as needed.
 */
class SCORE_PLUGIN_RECORDING_EXPORT CountInHold final : public Execution::ExecutionAction
{
  SCORE_CONCRETE("6d3e2a91-58c4-4f7b-b1a2-9c4e7f05d813")
public:
  CountInHold();
  ~CountInHold() override;
  static CountInHold* instance() noexcept;

  struct Count
  {
    Media::Metro::sound_set_ptr sounds;
    //! The device channels the clicks go to: left, then right
    std::vector<int> channels;
    float volume{1.f};
    bool accent{true};
    int64_t beatFrames{};
    int beats{};
    int beatsPerBar{4};
  };

  //! GUI thread, while no execution tick runs
  void arm(Count c);
  //! GUI thread: the count stops where it is
  void cancel() noexcept;
  //! The score is still waiting
  bool counting() const noexcept { return m_armed.load(std::memory_order_acquire); }
  //! The clicks played so far by the last count
  int clicked() const noexcept { return m_clicked.load(std::memory_order_relaxed); }

  void startTick(const ossia::audio_tick_state& st) override;
  bool holds() const noexcept override { return m_holding; }

private:
  void mix(const Media::Metro::click_sound& snd, int64_t from, const ossia::audio_tick_state& st,
           int64_t at, int64_t n) const noexcept;

  Count m_count;
  std::atomic_bool m_armed{};
  std::atomic_int m_clicked{};
  bool m_started{};
  bool m_holding{};
  int64_t m_lead{};
  int64_t m_pos{};
  int64_t m_end{};
};

/**
 * The global metronome: while the document plays, the beats of the root
 * interval are clicked on the cue output when the Metronome setting is on;
 * with "Count-in before playback", playback waits for its bars of clicks.
 */
class SCORE_PLUGIN_RECORDING_EXPORT Metronome final : public QObject
{
public:
  explicit Metronome(ApplicationPlugin& plug);
  ~Metronome() override;

  //! The beats of the root clicked while playing (setting "Metronome")
  bool enabled() const noexcept;
  //! The beats clicked since playback started
  int beats() const noexcept;

  //! Where the clicks go: the audio device's /out/cue, else /out/main
  static ossia::net::parameter_base* cueOutput(const score::DocumentContext& ctx);
  //! Its device channels; the first two when it has no mapping
  static std::vector<int> cueChannels(const score::DocumentContext& ctx);

  //! The tempo `played` is heard at from `date`: the tempo of its own or of
  //! the nearest box it is in, times its speed and the speeds of those boxes
  static double playedTempo(const Scenario::IntervalModel& played, TimeVal date);
  //! Its time signature then, or the nearest box's; 4/4 without any
  static ossia::time_signature
  playedSignature(const Scenario::IntervalModel& played, TimeVal date);

  //! The tempo and signature an interval's own curve and map give at `date`
  static double tempoAt(const Scenario::IntervalModel& root, TimeVal date);
  static ossia::time_signature signatureAt(const Scenario::IntervalModel& root, TimeVal date);

private:
  void aboutToPlay(const Scenario::IntervalModel& root, TimeVal date);
  void started();
  void stopped();
  void removeNode();

  ApplicationPlugin& m_plug;
  std::shared_ptr<std::atomic_bool> m_on = std::make_shared<std::atomic_bool>();
  std::shared_ptr<ossia::graph_node> m_node;
  const score::DocumentContext* m_playing{};
};
}
