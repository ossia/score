#pragma once
#include <Fx/Looper/MidiLoop.hpp>
#include <Fx/Looper/TakeWriter.hpp>

#include <libremidi/writer.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

namespace Nodes::Looper
{
//! What is heard of a MIDI loop, copied by the processing thread for a worker
//! to write it. Fixed storage: the copy does not allocate.
struct midi_export
{
  std::array<midi_note, midi_loop::max_notes> notes{};
  std::array<midi_control, midi_loop::max_controls> controls{};
  int note_count{};
  int control_count{};
  int64_t length{};
  double rate{48000.};
  double tempo{120.};
  int meter_numerator{4};
  int meter_denominator{4};
  bool wrap{true};

  static constexpr int ticks_per_quarter = 480;

  //! The loop as a Standard MIDI File, at 120 BPM: score reads a file's
  //! length at the file's tempo, and a quarter lasts half a second of score's
  //! time whatever the tempo. The file ends exactly at the loop's end.
  std::string to_smf() const
  {
    const double frames_per_quarter = rate * 60. / std::max(1., tempo);
    auto to_tick = [&](int64_t frames) {
      return int64_t(std::llround(frames / frames_per_quarter * ticks_per_quarter));
    };
    const int64_t end = std::max<int64_t>(1, to_tick(length));

    struct event
    {
      int64_t tick;
      int order; //!< note-offs, then the rest, at a given tick
      libremidi::message m;
    };
    std::vector<event> events;
    events.reserve(std::size_t(note_count) * 4 + std::size_t(control_count) + 4);

    events.push_back({0, 1, libremidi::meta_events::tempo(500000)});
    events.push_back(
        {0, 1, libremidi::meta_events::time_signature(meter_numerator, meter_denominator)});

    for(int i = 0; i < note_count; i++)
    {
      const auto& n = notes[i];
      const int64_t on = std::clamp<int64_t>(to_tick(n.start), 0, end - 1);
      const int64_t len = std::max<int64_t>(1, to_tick(std::max<int64_t>(1, n.length)));
      int64_t off = on + std::min(len, end - 1);
      const int ch = std::clamp<int>(n.channel, 1, 16);
      events.push_back(
          {on, 2, libremidi::channel_events::note_on(ch, n.pitch, std::max<int>(1, n.velocity))});
      if(off <= end)
      {
        events.push_back({off, 0, libremidi::channel_events::note_off(ch, n.pitch, 0)});
      }
      else if(wrap)
      {
        // The note carries on from the loop's start: its tail is a note of
        // its own there.
        events.push_back({end, 0, libremidi::channel_events::note_off(ch, n.pitch, 0)});
        events.push_back(
            {0, 2, libremidi::channel_events::note_on(ch, n.pitch, std::max<int>(1, n.velocity))});
        events.push_back(
            {std::max<int64_t>(1, off - end), 0, libremidi::channel_events::note_off(ch, n.pitch, 0)});
      }
      else
      {
        events.push_back({end, 0, libremidi::channel_events::note_off(ch, n.pitch, 0)});
      }
    }

    for(int i = 0; i < control_count; i++)
    {
      const auto& c = controls[i];
      const int64_t t = std::clamp<int64_t>(to_tick(c.pos), 0, end - 1);
      if((c.status & 0xF0) == 0xD0)
        events.push_back({t, 1, libremidi::message{c.status, c.data1}});
      else
        events.push_back({t, 1, libremidi::message{c.status, c.data1, c.data2}});
    }

    // A marker on the loop's end: the end of the track is the end of the
    // loop even when the last note ends earlier.
    events.push_back({end, 3, libremidi::message{0xFF, 0x06, 3, 'e', 'n', 'd'}});

    std::stable_sort(events.begin(), events.end(), [](const event& a, const event& b) {
      return a.tick != b.tick ? a.tick < b.tick : a.order < b.order;
    });

    libremidi::writer w;
    w.ticksPerQuarterNote = ticks_per_quarter;
    int64_t prev = 0;
    for(const auto& e : events)
    {
      w.add_event(int(e.tick - prev), 0, e.m);
      prev = e.tick;
    }

    std::ostringstream out;
    w.write(out);
    return std::move(out).str();
  }
};

struct midi_take_job
{
  std::shared_ptr<take_writer> writer;
  std::shared_ptr<midi_export> data;
  int slot{};
  //! Number of the take, 0 for a new one; -1 for the mix, else the pass.
  int number{};
  int layer{-1};

  bool run()
  {
    if(!writer || !data || data->length <= 0)
      return false;
    const std::string bytes = data->to_smf();
    const auto& path = writer->next_path(slot, number, layer);
    std::ofstream f{
        std::filesystem::path{std::u8string_view{
            reinterpret_cast<const char8_t*>(path.data()), path.size()}},
        std::ios::binary | std::ios::trunc};
    f.write(bytes.data(), std::streamsize(bytes.size()));
    return bool(f);
  }
};
}
