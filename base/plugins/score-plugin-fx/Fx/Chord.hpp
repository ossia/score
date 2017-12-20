#pragma once

#include <Engine/Node/PdNode.hpp>
#include <deque>
namespace Nodes
{
namespace Chord
{
struct Node
{
    struct Metadata
    {
        static const constexpr auto prettyName = "Chord";
        static const constexpr auto objectKey = "Chord";
        static const constexpr auto category = "Midi";
        static const constexpr auto tags = std::array<const char*, 0>{};
        static const constexpr auto uuid = make_uuid("F0904279-EA26-48DB-B0DF-F68FE3091DA1");
    };

    struct State
    {
        struct chord
        {
            std::string ch{};
            int notes{};
        };
        std::map<uint8_t, std::vector<chord>> chords;
    };
    enum Chord
    {
      I, II, III, IV, V, VI, VII
    };

    static const constexpr auto info =
        Process::create_node()
        .midi_ins({{"in"}})
        .midi_outs({{"out"}})
        .controls(Process::IntSlider{"Num. Notes", 1, 5, 3},
                  Process::make_enum(
                    "Chord",
                    0U,
                    Process::array("Major", "Minor", "Sus2", "Sus4", "Dim", "Aug"))
                  )
        .state<State>()
        .build();
    using control_policy = Process::DefaultTick;
    // C C# D D# E F F# G G# A A# B
    // 1 .  . .  1 . .  1 .  1 .  .
    static const constexpr std::array<int, 5> major7{ 0, 4, 7, 9, 12 };
    static const constexpr std::array<int, 5> minor7{ 0, 3, 7, 9, 12 };
    static const constexpr std::array<int, 5> sus2{ 0, 3, 7, 9, 12 };
    static const constexpr std::array<int, 5> sus4{ 0, 3, 7, 9, 12 };
    static const constexpr std::array<int, 5> dim{ 0, 3, 7, 9, 12 };
    static const constexpr std::array<int, 5> aug{ 0, 3, 7, 9, 12 };

    template<typename T>
    static void startChord(const T& chord, const mm::MidiMessage& m, const std::size_t num, ossia::midi_port& op)
    {
      for(std::size_t i = 0; i < std::min(num, chord.size()); i++)
      {
        auto new_note = m.data[1] + chord[i];
        if(new_note > 127)
          break;

        auto non = mm::MakeNoteOn(m.getChannel(), new_note, m.data[2]);
        non.timestamp = m.timestamp;
        op.messages.push_back(non);
      }
    }

    template<typename T>
    static void stopChord(const T& chord, const mm::MidiMessage& m, const std::size_t num, ossia::midi_port& op)
    {
      for(std::size_t i = 0; i < std::min(num, chord.size()); i++)
      {
        auto new_note = m.data[1] + chord[i];
        if(new_note > 127)
          break;

        auto noff = mm::MakeNoteOff(m.getChannel(), new_note, m.data[2]);
        noff.timestamp = m.timestamp;
        op.messages.push_back(noff);
      }
    }

    template<typename F>
    static void dispatchChord(std::string_view chord, const mm::MidiMessage& m, int num, ossia::midi_port& op, F&& f)
    {
      static const ossia::string_view_map<std::array<int, 5>> chords{
        {"Major", major7},
        {"Minor", minor7},
        {"Sus2", sus2},
        {"Sus4", sus4},
        {"Dim", dim},
        {"Aug", aug}
      };
      auto it = chords.find(chord);
      if(it != chords.end())
        f(it->second, m, num, op);
    }
    static void run(
        const ossia::midi_port& ip,
        const Control::timed_vec<int>& num,
        const Control::timed_vec<std::string>& chord,
        ossia::midi_port& op,
        ossia::time_value prev_date,
        ossia::token_request tk,
        ossia::execution_state& st,
        State& self)
    {
      for(const mm::MidiMessage& m : ip.messages)
      {
        auto lastNum = num.rbegin()->second;
        const auto& lastCh = chord.rbegin()->second;

        if(m.getMessageType() == mm::MessageType::NOTE_ON)
        {
          auto cur = m.data[1];
          self.chords[cur].push_back({ lastCh, lastNum });
          dispatchChord(lastCh, m, lastNum, op, [] (auto&&... args) { startChord(args...); });
        }
        else if(m.getMessageType() == mm::MessageType::NOTE_OFF)
        {
          auto it = self.chords.find(m.data[1]);
          if(it != self.chords.end())
          {
            for(const State::chord& chord : it->second)
            {
              dispatchChord(chord.ch, m, chord.notes, op, [] (auto&&... args) { stopChord(args...); });
            }
            it->second.clear();
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

using Factories = Process::Factories<Node>;
}
}
