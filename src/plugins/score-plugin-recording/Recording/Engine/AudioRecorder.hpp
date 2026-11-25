#pragma once
#include <Recording/Engine/StartGate.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/lockfree_queue.hpp>

#include <score_plugin_recording_export.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace Recording
{
/**
 * What an audio recorder and the session that owns it share.
 *
 * The audio thread fills blocks taken from a pool and hands them to a writer
 * thread, which gives them back: nothing is allocated or locked on the audio
 * thread. The session asks for a start or a stop, and reads where the take
 * is, through atomics.
 */
struct SCORE_PLUGIN_RECORDING_EXPORT AudioTake
{
  enum State : int
  {
    //! Not asked to record yet
    Idle,
    //! Asked to: waiting for its box to play and for the start date
    Waiting,
    Recording,
    //! Stopped; what was recorded is in the blocks given to the writer
    Done
  };

  struct Block
  {
    //! One channel after the other, `capacity` frames each
    std::vector<float> data;
    int64_t frames{};
    int channels{};
  };

  //! `channels` is the most a take keeps; it keeps as many as its source
  //! gives when the take starts (activeChannels).
  AudioTake(int channels, int blockFrames, int blockCount);
  ~AudioTake();
  AudioTake(const AudioTake&) = delete;
  AudioTake& operator=(const AudioTake&) = delete;

  const int channels;
  const int blockFrames;

  // GUI thread -> audio thread
  //! Grid the start lands on, as a token_request quantification rate: 0 starts
  //! at once, 1 on the next bar, 4 on the next quarter note.
  std::atomic<double> startRate{0.};
  //! Counted in: the bars that go by before the line it starts on
  std::atomic<double> leadBars{0.};
  //! Model duration after which the take stops on its own; 0: until asked.
  std::atomic<int64_t> length{0};
  //! Bars after which the take stops on its own, counted in the musical time
  //! of the box from where it starts (exact through tempo changes); 0: none.
  //! Takes precedence over `length`.
  std::atomic<double> lengthBars{0.};
  std::atomic<bool> startRequested{false};
  std::atomic<bool> stopRequested{false};

  // Audio thread -> GUI thread
  std::atomic<int> state{Idle};
  //! The date in the box of the first recorded frame
  std::atomic<int64_t> startDate{0};
  //! Its musical position, in quarters
  std::atomic<double> startPosition{0.};
  //! The date in the box after the last recorded frame
  std::atomic<int64_t> endDate{0};
  std::atomic<int64_t> frames{0};
  //! The channels recorded: the source's when the take started
  std::atomic<int> activeChannels{0};
  //! Frames that could not be recorded: no free block. Never silent.
  std::atomic<int64_t> droppedFrames{0};
  //! The tempo when the take started
  std::atomic<double> tempo{0.};
  //! The tempo changed while it recorded: a Sound take has one tempo
  std::atomic<bool> tempoChanged{false};
  //! It recorded at a speed other than 1
  std::atomic<bool> offSpeed{false};
  //! The meter when the take started
  std::atomic<int> meterNumerator{4};
  std::atomic<int> meterDenominator{4};

  // Audio thread
  ossia::spsc_queue<Block*> freeBlocks;
  // Writer thread
  ossia::spsc_queue<Block*> filledBlocks;

  //! Called by the writer thread for every filled block, in order; the block
  //! goes back to the pool afterwards. Returns how many blocks it handled.
  template <typename F>
  int drain(F&& write)
  {
    int n = 0;
    Block* b{};
    while(filledBlocks.try_dequeue(b))
    {
      write(*b);
      b->frames = 0;
      freeBlocks.try_enqueue(b);
      n++;
    }
    return n;
  }

private:
  std::vector<std::unique_ptr<Block>> m_pool;
};

/**
 * Records what reaches its inlet, in the time of a box: the clock is the node
 * of the box's interval, whose requested tokens say, every tick, which dates
 * of the box the buffer covers. Before the box plays, or before the start
 * date, nothing is recorded; the start lands exactly on the grid sample
 * (token_request::physical_position), and the take stops on the sample its
 * length ends at.
 */
class SCORE_PLUGIN_RECORDING_EXPORT audio_record_node final : public ossia::graph_node
{
public:
  audio_record_node(std::shared_ptr<AudioTake> take, std::shared_ptr<ossia::graph_node> clock);
  ~audio_record_node() override;

  std::string label() const noexcept override { return "audio recorder"; }
  //! Hands the block being filled to the writer. Only once the execution is
  //! over: the node is not run anymore.
  void flushStopped() noexcept { flush(); }
  void run(const ossia::token_request&, ossia::exec_state_facade e) noexcept override;

  const std::shared_ptr<AudioTake>& take() const noexcept { return m_take; }

private:
  void record(const ossia::audio_port& in, int64_t from, int64_t count) noexcept;
  void flush() noexcept;

  std::shared_ptr<AudioTake> m_take;
  std::shared_ptr<ossia::graph_node> m_clock;
  AudioTake::Block* m_current{};
  //! Where the take ends musically, in quarters; < 0: not by bars
  double m_endPosition{-1.};
  StartGate m_gate;
  //! The speed when the take started: the tempo ticks give follows it
  double m_startSpeed{1.};
  //! Ticks its box did not play, while it recorded
  int m_idleTicks{};
};
}
