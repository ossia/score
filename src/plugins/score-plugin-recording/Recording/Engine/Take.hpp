#pragma once
#include <atomic>
#include <cstdint>

namespace Recording
{
/**
 * What every take shares, whatever it records: the session asks for a start
 * or a stop and says when and how long, the audio thread says where the take
 * is. All of it through atomics: nothing is locked on the audio thread.
 */
struct TakeState
{
  enum State : int
  {
    //! Not asked to record yet
    Idle,
    //! Asked to: waiting for its box to play and for the start date
    Waiting,
    Recording,
    //! Stopped; what was recorded has been handed over
    Done
  };

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
  //! The date in the box of the first thing recorded
  std::atomic<int64_t> startDate{0};
  //! Its musical position, in quarters
  std::atomic<double> startPosition{0.};
  //! The date in the box after the last thing recorded
  std::atomic<int64_t> endDate{0};
  //! The tempo when the take started
  std::atomic<double> tempo{0.};
  //! The tempo changed while it recorded: a take has one tempo
  std::atomic<bool> tempoChanged{false};
  //! It recorded at a speed other than 1
  std::atomic<bool> offSpeed{false};
  //! The meter when the take started
  std::atomic<int> meterNumerator{4};
  std::atomic<int> meterDenominator{4};
};
}
