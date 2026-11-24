#pragma once
#include <Recording/Takes/WavStreamWriter.hpp>

#include <QPointer>
#include <QPointF>
#include <QSize>

#include <optional>

#include <cstdint>

namespace Process
{
class ProcessModel;
}
namespace Recording
{
//! How a video take is encoded
struct VideoSettings
{
  enum Codec
  {
    H264,
    H265,
    Hap,
    ProRes,
    Ffv1
  };
  QSize size{1920, 1080};
  int fps{30};
  Codec codec{H264};

  //! The file extension of the codec's container
  QString extension() const;
};

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
  //! A take of a length in bars is placed as a loop of that length, from the
  //! start of the box, turned so that each moment of the box hears what was
  //! played there
  bool loopFixedLength{true};
  //! MIDI takes: the start of each note moves to the nearest multiple of
  //! this many quarter notes in the box; 0: as played
  double midiQuantize{0.};
  //! Video takes
  VideoSettings video;

  //! Where the takes' nodes go in the node view: where the menu was opened;
  //! none, where a new process goes
  std::optional<QPointF> nodePosition;
  //! The metronome clicks the box's beats on the main output while the takes
  //! wait for their start (with a start on the next bar: a count-in)
  bool countIn{false};
  //! When a take is placed, the process it was recorded from is muted in the
  //! same step, so that the take is not heard twice
  bool muteSource{false};
  WavStreamWriter::Format audioFormat{WavStreamWriter::Format::Float32};
  Target target{Target::ThisBox};

  //! How a take recorded over several passes of the same length is kept
  enum class Passes
  {
    //! One take, however long
    One,
    //! Each pass a loop of the box, in one slot: the last plays, the others
    //! are muted, to be chosen from
    Alternatives,
    //! Each pass where it was recorded
    SideBySide,
    //! Only the last pass, as a loop
    LastOnly
  };
  Passes passes{Passes::One};
  //! The length of a pass, in bars of the box
  double passBars{1.};

  //! Target::Into: the piano roll a MIDI take, or the automation a value take
  //! is recorded into
  QPointer<const Process::ProcessModel> into;
  //! Target::Into: the notes it had over the span recorded are removed first;
  //! else the take's notes are added to them
  bool replace{false};
  //! Target::Into an automation: its curve is replaced only where values
  //! came, each time until `touchRelease` (flicks) after the last one; it plays
  //! meanwhile. Else (latch) from the first value to the end, silent meanwhile.
  bool touch{false};

  //! Free space the disk of the Takes folder needs for a recording to start;
  //! it stops, its takes kept, when less than a quarter of it is left
  int64_t minFreeBytes{int64_t(512) << 20};

  //! Kept in the background while playing: capture() places the last
  //! passBars whole bars of it as loops, else nothing is kept
  bool capture{false};
  int64_t touchRelease{352800000};
};
}
