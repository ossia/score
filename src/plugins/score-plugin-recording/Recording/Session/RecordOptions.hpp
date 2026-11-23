#pragma once
#include <Recording/Takes/WavStreamWriter.hpp>

#include <cstdint>

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
    FileOnly
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
};
}
