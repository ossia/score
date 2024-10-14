#pragma once
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
  using midi_out = halp::midi_out_bus<"out", libremidi::message>;
  struct
  {
    halp::midi_bus<"in", libremidi::message> midi;
    halp::hslider_i32<"Num. Notes", halp::irange{1, 5, 3}> num;
    halp::enum_t<Chord, "Chord"> chord;
  } inputs;
  struct
  {
    midi_out midi;
  } outputs;
  struct chord_type
  {
    std::string_view ch{};
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
  static void startChord(const T& chord, const libremidi::message& m, const std::size_t num, midi_out& op)
  {
    for(std::size_t i = 0; i < std::min(num, chord.size()); i++)
    {
      auto new_note = m.bytes[1] + chord[i];
      if(new_note > 127)
        break;

      op.note_on(m.get_channel(), new_note, m.bytes[2]).timestamp = m.timestamp;
    }
  }

  template <typename T>
  static void stopChord(const T& chord, const libremidi::message& m, const std::size_t num, midi_out& op)
  {
    for(std::size_t i = 0; i < std::min(num, chord.size()); i++)
    {
      auto new_note = m.bytes[1] + chord[i];
      if(new_note > 127)
        break;

      op.note_off(m.get_channel(), new_note, m.bytes[2]).timestamp = m.timestamp;
    }
  }

  template <typename F>
  static void dispatchChord(std::string_view chord, const libremidi::message& m, int num, midi_out& op, F&& f)
  {
    static const ossia::string_view_map<std::array<int, 5>> chords{{"Maj", major7}, {"Min", minor7}, {"Sus2", sus2}, {"Sus4", sus4}, {"Dim", dim}, {"Aug", aug}};
    auto it = chords.find(chord);
    if(it != chords.end())
      f(it->second, m, num, op);
  }

  // FIXME here we want precise ticking, e.g. call for each change in any parameter
  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& tk)
  {
    for(const libremidi::message& m : inputs.midi)
    {
      int lastNum = inputs.num;
      std::string_view lastCh = magic_enum::enum_name<Chord>(inputs.chord);
      if(m.get_message_type() == libremidi::message_type::NOTE_ON)
      {
        auto cur = m.bytes[1];
        this->chords[cur].push_back({lastCh, lastNum});
        dispatchChord(lastCh, m, lastNum, outputs.midi, [](auto&&... args) { startChord(args...); });
      }
      else if(m.get_message_type() == libremidi::message_type::NOTE_OFF)
      {
        auto it = this->chords.find(m.bytes[1]);
        if(it != this->chords.end())
        {
          for(const auto& chord : it->second)
          {
            dispatchChord(chord.ch, m, chord.notes, outputs.midi, [](auto&&... args) { stopChord(args...); });
          }
          const_cast<std::vector<chord_type>&>(it->second).clear();
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
