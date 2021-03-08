#pragma once

#include <Engine/Node/PdNode.hpp>

#include <deque>
namespace Nodes
{
namespace Chord
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Chord";
    static const constexpr auto objectKey = "Chord";
    static const constexpr auto category = "Midi";
    static const constexpr auto author = "ossia score";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto kind = Process::ProcessCategory::Mapping;
    static const constexpr auto description = "Generate a chord from a single note";
    static const uuid_constexpr auto uuid = make_uuid("F0904279-EA26-48DB-B0DF-F68FE3091DA1");

    static const constexpr midi_in midi_ins[]{"in"};
    static const constexpr midi_out midi_outs[]{"out"};
    static const constexpr auto controls = std::make_tuple(
        Control::IntSlider{"Num. Notes", 1, 5, 3},
        Control::make_enum(
            "Chord",
            0U,
            ossia::make_array("Maj", "Min", "Sus2", "Sus4", "Dim", "Aug")));
  };

  struct State
  {
    struct chord
    {
      std::string ch{};
      int notes{};
    };
    ossia::flat_map<uint8_t, std::vector<chord>> chords;
  };

  enum Chord
  {
    I,
    II,
    III,
    IV,
    V,
    VI,
    VII
  };

  using control_policy = ossia::safe_nodes::default_tick;
  // C C# D D# E F F# G G# A A# B
  // 1 .  . .  1 . .  1 .  1 .  .
  static const constexpr std::array<int, 5> major7{0, 4, 7, 11, 12};
  static const constexpr std::array<int, 5> minor7{0, 3, 7, 10, 12};
  static const constexpr std::array<int, 5> sus2{0, 2, 7, 9, 12};
  static const constexpr std::array<int, 5> sus4{0, 5, 7, 9, 12};
  static const constexpr std::array<int, 5> dim{0, 3, 6, 9, 12};
  static const constexpr std::array<int, 5> aug{0, 4, 8, 10, 12};

  template <typename T>
  static void
  startChord(const T& chord, const libremidi::message& m, const std::size_t num, ossia::midi_port& op)
  {
    for (std::size_t i = 0; i < std::min(num, chord.size()); i++)
    {
      auto new_note = m.bytes[1] + chord[i];
      if (new_note > 127)
        break;

      auto non = libremidi::message::note_on(m.get_channel(), new_note, m.bytes[2]);
      non.timestamp = m.timestamp;
      op.messages.push_back(non);
    }
  }

  template <typename T>
  static void
  stopChord(const T& chord, const libremidi::message& m, const std::size_t num, ossia::midi_port& op)
  {
    for (std::size_t i = 0; i < std::min(num, chord.size()); i++)
    {
      auto new_note = m.bytes[1] + chord[i];
      if (new_note > 127)
        break;

      auto noff = libremidi::message::note_off(m.get_channel(), new_note, m.bytes[2]);
      noff.timestamp = m.timestamp;
      op.messages.push_back(noff);
    }
  }

  template <typename F>
  static void dispatchChord(
      std::string_view chord,
      const libremidi::message& m,
      int num,
      ossia::midi_port& op,
      F&& f)
  {
    static const ossia::string_view_map<std::array<int, 5>> chords{
        {"Maj", major7},
        {"Min", minor7},
        {"Sus2", sus2},
        {"Sus4", sus4},
        {"Dim", dim},
        {"Aug", aug}};
    auto it = chords.find(chord);
    if (it != chords.end())
      f(it->second, m, num, op);
  }
  static void
  run(const ossia::midi_port& ip,
      const ossia::safe_nodes::timed_vec<int>& num,
      const ossia::safe_nodes::timed_vec<std::string>& chord,
      ossia::midi_port& op,
      ossia::token_request tk,
      ossia::exec_state_facade st,
      State& self)
  {
    for (const libremidi::message& m : ip.messages)
    {
      auto lastNum = num.rbegin()->second;
      const auto& lastCh = chord.rbegin()->second;

      if (m.get_message_type() == libremidi::message_type::NOTE_ON)
      {
        auto cur = m.bytes[1];
        self.chords[cur].push_back({lastCh, lastNum});
        dispatchChord(lastCh, m, lastNum, op, [](auto&&... args) { startChord(args...); });
      }
      else if (m.get_message_type() == libremidi::message_type::NOTE_OFF)
      {
        auto it = self.chords.find(m.bytes[1]);
        if (it != self.chords.end())
        {
          for (const State::chord& chord : it->second)
          {
            dispatchChord(
                chord.ch, m, chord.notes, op, [](auto&&... args) { stopChord(args...); });
          }
          const_cast<std::vector<State::chord>&>(it->second).clear();
        }
      }
      else
      {
        // just forward
        op.messages.push_back(m);
      }
    }
  }
};
}
}
