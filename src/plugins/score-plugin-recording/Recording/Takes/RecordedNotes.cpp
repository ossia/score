#include <ossia/detail/flicks.hpp>
#include <Process/TakeMidiFile.hpp>
#include "RecordedNotes.hpp"

#include <libremidi/cmidi2.hpp>
#include <libremidi/message.hpp>
#include <libremidi/writer.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <sstream>

namespace Recording
{
namespace
{
struct NoteEvent
{
  bool on{};
  uint8_t channel{};
  uint8_t pitch{};
  uint8_t velocity{};
};

std::optional<NoteEvent> noteEvent(const libremidi::ump& u)
{
  const auto* p = u.data;
  const auto type = cmidi2_ump_get_message_type(p);
  const auto status = cmidi2_ump_get_status_code(p);
  if(type == CMIDI2_MESSAGE_TYPE_MIDI_1_CHANNEL)
  {
    const uint8_t pitch = cmidi2_ump_get_midi1_note_note(p);
    const uint8_t vel = cmidi2_ump_get_midi1_note_velocity(p);
    if(status == CMIDI2_STATUS_NOTE_ON)
      return NoteEvent{vel > 0, cmidi2_ump_get_channel(p), pitch, vel};
    if(status == CMIDI2_STATUS_NOTE_OFF)
      return NoteEvent{false, cmidi2_ump_get_channel(p), pitch, vel};
  }
  else if(type == CMIDI2_MESSAGE_TYPE_MIDI_2_CHANNEL)
  {
    const uint8_t pitch = cmidi2_ump_get_midi2_note_note(p);
    const uint16_t vel16 = cmidi2_ump_get_midi2_note_velocity(p);
    // A MIDI 1 note on of velocity 0, the usual note off of keyboards, comes
    // translated as a MIDI 2 note on of velocity 0: it ends the note.
    if(status == CMIDI2_STATUS_NOTE_ON && vel16 == 0)
      return NoteEvent{false, cmidi2_ump_get_channel(p), pitch, 0};
    // 16 bits down to 7, never 0 for a note on
    const uint8_t vel = uint8_t(std::max(1, vel16 >> 9));
    if(status == CMIDI2_STATUS_NOTE_ON)
      return NoteEvent{true, cmidi2_ump_get_channel(p), pitch, vel};
    if(status == CMIDI2_STATUS_NOTE_OFF)
      return NoteEvent{false, cmidi2_ump_get_channel(p), pitch, vel};
  }
  return std::nullopt;
}
}

std::vector<RecordedNote>
notesOf(const std::vector<MidiTake::Event>& events, int64_t endDate)
{
  std::vector<RecordedNote> notes;
  // Index of the note sounding on each channel and pitch, or -1
  std::array<std::array<int, 128>, 16> sounding;
  for(auto& c : sounding)
    c.fill(-1);

  for(const auto& ev : events)
  {
    const auto n = noteEvent(ev.message);
    if(!n || n->pitch > 127 || n->channel > 15)
      continue;
    int& idx = sounding[n->channel][n->pitch];
    if(idx >= 0)
    {
      // Ended by its note off, or by the same note played again
      auto& note = notes[idx];
      note.duration = std::max<int64_t>(0, ev.date - note.start);
      idx = -1;
    }
    if(n->on)
    {
      idx = int(notes.size());
      notes.push_back(RecordedNote{ev.date, 0, n->pitch, n->velocity, n->channel});
    }
  }

  for(auto& c : sounding)
    for(int idx : c)
      if(idx >= 0)
        notes[idx].duration = std::max<int64_t>(0, endDate - notes[idx].start);

  std::stable_sort(notes.begin(), notes.end(), [](const auto& a, const auto& b) {
    return a.start < b.start;
  });
  return notes;
}

void quantizeNotes(std::vector<RecordedNote>& notes, double quarters)
{
  if(!(quarters > 0.))
    return;
  const double grid = quarters * ossia::quarter_duration<double>;
  for(auto& n : notes)
    n.start = int64_t(std::llround(std::round(double(n.start) / grid) * grid));
  std::stable_sort(notes.begin(), notes.end(), [](auto& a, auto& b) { return a.start < b.start; });
}

std::string smfOf(
    const std::vector<RecordedNote>& notes, int64_t start, int64_t length, double tempo,
    int meterNumerator, int meterDenominator)
{
  constexpr int ticks_per_quarter = Process::takeTicksPerQuarter;
  constexpr double flicks_per_minute = ossia::flicks_per_second<double> * 60.;
  const double bpm = tempo > 0. ? tempo : 120.;
  const double flicks_per_quarter = flicks_per_minute / bpm;
  auto to_tick = [&](int64_t flicks) {
    return int64_t(std::llround(double(flicks) / flicks_per_quarter * ticks_per_quarter));
  };
  const int64_t end = std::max<int64_t>(1, to_tick(length));

  std::vector<Process::TakeMidiEvent> events;
  events.reserve(notes.size() * 2 + 3);

  for(const auto& n : notes)
  {
    const int ch = std::clamp<int>(n.channel + 1, 1, 16);
    const int64_t on = std::clamp<int64_t>(to_tick(n.start - start), 0, end);
    const int64_t off
        = std::clamp<int64_t>(on + std::max<int64_t>(1, to_tick(n.duration)), on, end);
    events.push_back(
        {on, 2, libremidi::channel_events::note_on(ch, n.pitch, std::max<int>(1, n.velocity))});
    events.push_back({off, 0, libremidi::channel_events::note_off(ch, n.pitch, 0)});
  }
  return Process::takeMidiFile(
      std::move(events), end, bpm, meterNumerator, meterDenominator);
}
}
