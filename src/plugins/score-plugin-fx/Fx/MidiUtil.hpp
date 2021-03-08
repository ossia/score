#pragma once
#include <Engine/Node/PdNode.hpp>
#undef slots
#include <frozen/unordered_map.h>
namespace Nodes::MidiUtil
{
using Note = Control::Note;
enum scale : int8_t
{
  all,
  ionian,
  dorian,
  phyrgian,
  lydian,
  mixolydian,
  aeolian,
  locrian,

  I,
  II,
  III,
  IV,
  V,
  VI,
  VII,
  custom,

  SCALES_MAX // always at end, used for counting
};

template <typename T>
constexpr void constexpr_swap(T& a, T& b)
{
  T tmp = a;
  a = b;
  b = tmp;
}

template <typename Iterator>
constexpr void constexpr_rotate(Iterator first, Iterator middle, Iterator last)
{
  using namespace std;
  Iterator next = middle;
  while (first != next)
  {
    constexpr_swap(*first++, *next++);
    if (next == last)
      next = middle;
    else if (first == middle)
      middle = next;
  }
}

using scale_array = std::array<bool, 128>;
using scales_array = std::array<scale_array, 12>;
constexpr scales_array make_scale(std::initializer_list<bool> notes)
{
  std::array<scale_array, 12> r{};
  for (std::size_t octave = 0; octave < 11; octave++)
  {
    std::size_t pos = 0;
    for (bool note : notes)
    {
      if (octave * 12 + pos < 128)
      {
        r[0][octave * 12 + pos] = note;
        pos++;
      }
    }
  }

  for (std::size_t octave = 1; octave < 12; octave++)
  {
    r[octave] = r[0];
    constexpr_rotate(r[octave].rbegin(), r[octave].rbegin() + octave, r[octave].rend());
  }
  return r;
}

constexpr bool is_same(std::string_view lhs, std::string_view rhs)
{
  if (lhs.size() == rhs.size())
  {
    for (std::size_t i = 0; i < lhs.size(); i++)
    {
      if (lhs[i] != rhs[i])
        return false;
    }
    return true;
  }
  return false;
}

constexpr int get_scale(std::string_view s)
{
  using namespace std::literals;
  if (is_same(s, std::string_view("all")))
    return scale::all;
  else if (is_same(s, std::string_view("ionian")))
    return scale::ionian;
  else if (is_same(s, std::string_view("dorian")))
    return scale::dorian;
  else if (is_same(s, std::string_view("phyrgian")))
    return scale::phyrgian;
  else if (is_same(s, std::string_view("lydian")))
    return scale::lydian;
  else if (is_same(s, std::string_view("mixolydian")))
    return scale::mixolydian;
  else if (is_same(s, std::string_view("aeolian")))
    return scale::aeolian;
  else if (is_same(s, std::string_view("locrian")))
    return scale::locrian;
  else if (is_same(s, std::string_view("I")))
    return scale::I;
  else if (is_same(s, std::string_view("II")))
    return scale::II;
  else if (is_same(s, std::string_view("III")))
    return scale::III;
  else if (is_same(s, std::string_view("IV")))
    return scale::IV;
  else if (is_same(s, std::string_view("V")))
    return scale::V;
  else if (is_same(s, std::string_view("VI")))
    return scale::VI;
  else if (is_same(s, std::string_view("VII")))
    return scale::VII;
  else
    return scale::custom;
}
#if defined(_MSC_VER)
#define MSVC_CONSTEXPR const
#else
#define MSVC_CONSTEXPR constexpr
#endif
static MSVC_CONSTEXPR frozen::unordered_map<int, scales_array, scale::SCALES_MAX - 1> scales{
    //                                C   D   E F   G   A   B
    {scale::all, make_scale({1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1})},
    {scale::ionian, make_scale({1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1})},
    {scale::dorian, make_scale({1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0})},
    {scale::phyrgian, make_scale({1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0})},
    {scale::lydian, make_scale({1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1})},
    {scale::mixolydian, make_scale({1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0})},
    {scale::aeolian, make_scale({1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0})},
    {scale::locrian, make_scale({1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0})},
    {scale::I, make_scale({1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0})},
    {scale::II, make_scale({0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0})},
    {scale::III, make_scale({0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1})},
    {scale::IV, make_scale({1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0})},
    {scale::V, make_scale({0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1})},
    {scale::VI, make_scale({1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0})},
    {scale::VII, make_scale({0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1})}};

static std::optional<std::size_t> find_closest_index(const scale_array& arr, std::size_t i)
{
  if (arr[i] == 1)
    return i;

  switch (i)
  {
    case 0:
      while (i != 12)
      {
        i++;
        if (arr[i] == 1)
          return i;
      }
      break;

    case 12:
      while (i != 0)
      {
        i--;
        if (arr[i] == 1)
          return i;
      }
      break;

    default:
    {
      std::size_t r = 0;
      while ((i - r) != 0 && (i + r) != 12)
      {
        if (arr[i + r] == 1)
          return i + r;
        else if (arr[i - r] == 1)
          return i - r;
        r++;
      }

      break;
    }
  }

  return std::nullopt;
}

struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Midi scale";
    static const constexpr auto objectKey = "MidiScale";
    static const constexpr auto category = "Midi";
    static const constexpr auto author = "ossia score";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto kind = Process::ProcessCategory::MidiEffect;
    static const constexpr auto description = "Maps a midi input to a given scale";
    static const uuid_constexpr auto uuid = make_uuid("06b33b83-bb67-4f7a-9980-f5d66e4266c5");

    static const constexpr midi_in midi_ins[]{"in"};
    static const constexpr midi_out midi_outs[]{"out"};
    static const constexpr auto controls = std::make_tuple(
        Control::make_unvalidated_enum(
            "Scale",
            0U,
            ossia::make_array(
                "all",
                "ionian",
                "dorian",
                "phyrgian",
                "lydian",
                "mixolydian",
                "aeolian",
                "locrian",
                "I",
                "II",
                "III",
                "IV",
                "V",
                "VI",
                "VII")),
        Control::Widgets::OctaveSlider("Base", 0, 1),
        Control::Widgets::OctaveSlider("Transpose", -4, 4));
  };

  struct State
  {
    ossia::flat_map<uint8_t, Note> map;
    std::string scale{};
    int base{};
    int transpose{};
  };

  static void exec(
      const ossia::midi_port& midi_in,
      const scale_array& scale,
      int transp,
      ossia::midi_port& midi_out,
      const int64_t offset,
      State& self)
  {
    for (const auto& msg : midi_in.messages)
    {
      switch (msg.get_message_type())
      {
        case libremidi::message_type::NOTE_ON:
        {
          // map to scale
          if (auto index = find_closest_index(scale, msg.bytes[1]))
          {
            // transpose
            auto res = msg;
            res.bytes[1] = (uint8_t)ossia::clamp(int(*index + transp), 0, 127);
            Note note{(uint8_t)res.bytes[1], (uint8_t)res.bytes[2], (uint8_t)res.get_channel()};
            auto it = self.map.find(msg.bytes[1]);
            if (it != self.map.end())
            {
              midi_out.messages.push_back(
                  libremidi::message::note_off(res.get_channel(), it->second.pitch, res.bytes[2]));
              midi_out.messages.back().timestamp = offset;
              midi_out.messages.push_back(res);
              midi_out.messages.back().timestamp = offset + 1;
              const_cast<Note&>(it->second) = note;
            }
            else
            {
              midi_out.messages.push_back(res);
              midi_out.messages.back().timestamp = offset;
              self.map.insert(std::make_pair((uint8_t)msg.bytes[1], note));
            }
          }
          break;
        }
        case libremidi::message_type::NOTE_OFF:
        {
          auto it = self.map.find(msg.bytes[1]);
          if (it != self.map.end())
          {
            midi_out.messages.push_back(
                libremidi::message::note_off(msg.get_channel(), it->second.pitch, msg.bytes[2]));
            midi_out.messages.back().timestamp = offset;
            self.map.erase(it);
          }
          break;
        }
        default:
          midi_out.messages.push_back(msg);
          break;
      }
    }
  }

  static void update(
      const ossia::midi_port& midi_in,
      const scale_array& scale,
      int transp,
      ossia::midi_port& midi_out,
      const int64_t offset,
      State& self)
  {
    for (auto& notes : self.map)
    {
      Note& note = const_cast<Note&>(notes.second);
      if (auto index = find_closest_index(scale, notes.first))
      {
        if ((*index + transp) != note.pitch)
        {
          midi_out.messages.push_back(libremidi::message::note_off(note.chan, note.pitch, note.vel));
          note.pitch = *index + transp;
          midi_out.messages.back().timestamp = offset;
          midi_out.messages.push_back(libremidi::message::note_on(note.chan, note.pitch, note.vel));
          midi_out.messages.back().timestamp = offset + 1;
        }
      }
    }
  }

  using control_policy = ossia::safe_nodes::default_tick;
  static void
  run(const ossia::midi_port& midi_in,
      const ossia::safe_nodes::timed_vec<std::string>& sc,
      const ossia::safe_nodes::timed_vec<int>& base,
      const ossia::safe_nodes::timed_vec<int>& transp,
      ossia::midi_port& midi_out,
      ossia::token_request tk,
      ossia::exec_state_facade st,
      State& self)
  {
    const auto& new_scale = sc.rbegin()->second;
    const int new_base = base.rbegin()->second;
    const int new_transpose = transp.rbegin()->second;
    std::string_view scale{new_scale.data(), new_scale.size()};

    const auto new_scale_idx = get_scale(scale);

    const auto tick_start = st.physical_start(tk);

    auto apply = [&](auto f) {
      if (new_scale_idx != scale::custom)
      {
        f(midi_in, scales.at(new_scale_idx)[new_base], new_transpose, midi_out, tick_start, self);
      }
      else
      {
        scale_array arr{{}};
        for (int oct = 0; oct < 10; oct++)
        {
          for (int i = 0; i < std::min((int)scale.size(), 12); i++)
          {
            arr[oct * 12 + i] = (scale[i] == '1');
          }
        }
        f(midi_in, arr, new_transpose, midi_out, tick_start, self);
      }
    };

    if (!self.map.empty()
        && (new_scale != self.scale || new_base != self.base || new_transpose != self.transpose))
    {
      apply(update);
    }

    apply(exec);

    self.scale = new_scale;
    self.base = new_base;
    self.transpose = new_transpose;
  }
};

}

namespace Nodes::PitchToValue
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "MIDI Pitch";
    static const constexpr auto objectKey = "PitchToValue";
    static const constexpr auto category = "Midi";
    static const constexpr auto author = "ossia score";
    static const constexpr auto kind = Process::ProcessCategory::MidiEffect;
    static const constexpr auto description = "Extract a MIDI pitch";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const uuid_constexpr auto uuid = make_uuid("29ce484f-cb56-4501-af79-88768fa261c3");

    static const constexpr midi_in midi_ins[]{"in"};
    static const constexpr value_out value_outs[]{{"out", "midipitch"}};
  };

  using control_policy = ossia::safe_nodes::default_tick;
  static void
  run(const ossia::midi_port& in,
      ossia::value_port& res,
      ossia::token_request tk,
      ossia::exec_state_facade st)
  {
    for (const auto& note : in.messages)
    {
      if (note.get_message_type() == libremidi::message_type::NOTE_ON)
        res.write_value(ossia::value{(int)note.bytes[1]}, note.timestamp);
    }
  }
};
}
