#pragma once
#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/string_map.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <libremidi/message.hpp>

namespace Nodes
{
namespace Chord
{
struct Node
{
  halp_meta(name, "Chord")
  halp_meta(c_name, "Chord")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-utilities.html#chord")
  halp_meta(description, "Generate a chord from a single note")
  halp_meta(uuid, "F0904279-EA26-48DB-B0DF-F68FE3091DA1");

  enum Chord
  {
    Maj,
    Min,
    Sus2,
    Sus4,
    Dim,
    Aug
  };
  using midi_out = halp::midi_out_bus<"out", halp::midi_msg>;
  struct
  {
    halp::midi_bus<"in", halp::midi_msg> midi;
    halp::hslider_i32<"Num. Notes", halp::irange{1, 5, 3}> num;
    halp::enum_t<Chord, "Chord"> chord;
  } inputs;
  struct
  {
    midi_out midi;
  } outputs;
  struct chord_type
  {
    Chord ch{};
    int notes{};
  };
  ossia::flat_map<uint8_t, std::vector<chord_type>> chords;

  // C C# D D# E F F# G G# A A# B
  // 1 .  . .  1 . .  1 .  1 .  .
  static const constexpr std::array<int, 5> major7{0, 4, 7, 11, 12};
  static const constexpr std::array<int, 5> minor7{0, 3, 7, 10, 12};
  static const constexpr std::array<int, 5> sus2{0, 2, 7, 9, 12};
  static const constexpr std::array<int, 5> sus4{0, 5, 7, 9, 12};
  static const constexpr std::array<int, 5> dim{0, 3, 6, 9, 12};
  static const constexpr std::array<int, 5> aug{0, 4, 8, 10, 12};

  template <typename T>
  static void startChord(
      const T& chord, const halp::midi_msg& m, const std::size_t num, midi_out& op)
  {
    for(std::size_t i = 0; i < std::min(num, chord.size()); i++)
    {
      auto new_note = m.bytes[1] + chord[i];
      if(new_note > 127)
        break;

      op.note_on(1, new_note, m.bytes[2]).timestamp = m.timestamp;
    }
  }

  template <typename T>
  static void
  stopChord(const T& chord, const halp::midi_msg& m, const std::size_t num, midi_out& op)
  {
    for(std::size_t i = 0; i < std::min(num, chord.size()); i++)
    {
      auto new_note = m.bytes[1] + chord[i];
      if(new_note > 127)
        break;

      op.note_off(1, new_note, m.bytes[2]).timestamp = m.timestamp;
    }
  }

  template <typename F>
  static void
  dispatchChord(Chord chord, const halp::midi_msg& m, int num, midi_out& op, F&& f)
  {
    switch(chord)
    {
      case Chord::Maj:
        f(major7, m, num, op);
        break;
      case Chord::Min:
        f(minor7, m, num, op);
        break;
      case Chord::Sus2:
        f(sus2, m, num, op);
        break;
      case Chord::Sus4:
        f(sus4, m, num, op);
        break;
      case Chord::Dim:
        f(dim, m, num, op);
        break;
      case Chord::Aug:
        f(aug, m, num, op);
        break;
      default:
        return;
    }
  }

  // FIXME here we want precise ticking, e.g. call for each change in any parameter
  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& tk)
  {
    for(const halp::midi_msg& m : inputs.midi)
    {
      int lastNum = inputs.num;

      if((m.bytes[0] & 0xF0) == 0x90) // note on
      {
        auto cur = m.bytes[1];
        this->chords[cur].push_back({inputs.chord, lastNum});
        dispatchChord(inputs.chord, m, lastNum, outputs.midi, [](auto&&... args) {
          startChord(args...);
        });
      }
      else if((m.bytes[0] & 0xF0) == 0x80) // note off
      {
        auto it = this->chords.find(m.bytes[1]);
        if(it != this->chords.end())
        {
          for(const auto& chord : it->second)
          {
            dispatchChord(chord.ch, m, chord.notes, outputs.midi, [](auto&&... args) { stopChord(args...); });
          }
          const_cast<std::vector<chord_type>&>(it->second).clear();
          this->chords.erase(it);
        }
      }
      else
      {
        // just forward
        outputs.midi.push_back(m);
      }
    }
  }
};
}
}
