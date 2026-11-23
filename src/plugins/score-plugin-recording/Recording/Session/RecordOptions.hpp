#pragma once
#include <Recording/Takes/WavStreamWriter.hpp>

#include <QPointer>

#include <cstdint>

namespace Process
{
class ProcessModel;
}
namespace Recording
{
//! How a recording session starts, stops, writes and places its takes.
struct RecordOptions
{
  enum class Start
  {
    //! At once; playback is started if it does not play
    Now,
    //! When the user starts playback
    OnPlay,
    //! On the first message (MIDI); audio starts at once
    FirstEvent
  };
  enum class Target
  {
    //! A new process in the box the source plays in
    ThisBox,
    //! Only the file, in Takes
    FileOnly,
    //! Into an existing process of the box: `into`
    Into
  };

  Start start{Start::Now};
  //! Grid of the start, as a token_request quantification rate: 0 at once,
  //! 1 on the next bar, 4 on the next quarter note.
  double startRate{0.};
  //! Model duration after which the take stops by itself; 0 until stopped.
  int64_t length{0};
  //! Bars after which the take stops by itself, counted in the box's musical
  //! time from where it starts; 0: none. Takes precedence over `length`.
  double lengthBars{0.};
  WavStreamWriter::Format audioFormat{WavStreamWriter::Format::Float32};
  Target target{Target::ThisBox};

  //! Target::Into: the piano roll a MIDI take, or the automation a value take
  //! is recorded into
  QPointer<const Process::ProcessModel> into;
  //! Target::Into: the notes it had over the span recorded are removed first;
  //! else the take's notes are added to them
  bool replace{false};
};
}
