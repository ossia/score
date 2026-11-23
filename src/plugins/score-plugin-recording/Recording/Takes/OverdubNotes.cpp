#include "OverdubNotes.hpp"

#include <Midi/MidiProcess.hpp>

#include <algorithm>

namespace Recording
{
NoteOverdub overdubNotes(
    const Midi::ProcessModel& into, const std::vector<RecordedNote>& notes,
    int64_t start, int64_t end, bool replace)
{
  NoteOverdub r;
  const double duration = double(into.duration().impl);
  if(duration <= 0.)
    return r;
  const bool loops = into.loops() && into.loopDuration().impl > 0;
  const int64_t loop = into.loopDuration().impl;
  const int64_t offset = into.startOffset().impl;

  // Where a date of the box is in the piano roll's content
  const auto content = [&](int64_t date) -> int64_t {
    if(loops)
      date = ((date % loop) + loop) % loop;
    return date + offset;
  };

  for(const auto& n : notes)
  {
    int64_t s = content(n.start);
    int64_t len = n.duration;
    if(loops)
    {
      const int64_t loop_end = loop + offset;
      len = std::min(len, loop_end - s);
    }
    if(len <= 0 || s + len <= 0)
      continue;
    r.add.emplace_back(
        double(s) / duration, double(len) / duration, n.pitch, n.velocity);
  }

  if(replace && end > start)
  {
    // The span recorded, in the piano roll's time; round a loop, all of it
    // when the take is at least a loop long
    const bool whole = loops && end - start >= loop;
    const double from = double(content(start)) / duration;
    const double to = double(content(end)) / duration;
    for(const auto& note : into.notes)
    {
      bool in = whole;
      if(!in)
      {
        if(from <= to)
          in = note.end() > from && note.start() < to;
        else // the span goes round the loop
          in = note.end() > from || note.start() < to;
      }
      if(in)
        r.remove.push_back(note.id());
    }
  }
  return r;
}
}
