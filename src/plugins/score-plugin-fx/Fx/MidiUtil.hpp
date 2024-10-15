#pragma once
#include <Fx/Types.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

namespace Nodes::MidiUtil
{
enum scale_type : int8_t
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
}

template <>
struct magic_enum::customize::enum_range<Nodes::MidiUtil::scale_type>
{
  static constexpr int min = 0;
  static constexpr int max = Nodes::MidiUtil::custom;
};

namespace Nodes::MidiUtil
{

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
  while(first != next)
  {
    constexpr_swap(*first++, *next++);
    if(next == last)
      next = middle;
    else if(first == middle)
      middle = next;
  }
}

using scale_array = std::array<bool, 128>;
using scales_array = std::array<scale_array, 12>;
constexpr scales_array make_scale(std::initializer_list<bool> notes)
{
  std::array<scale_array, 12> r{};
  for(std::size_t octave = 0; octave < 11; octave++)
  {
    std::size_t pos = 0;
    for(bool note : notes)
    {
      if(octave * 12 + pos < 128)
      {
        r[0][octave * 12 + pos] = note;
        pos++;
      }
    }
  }

  for(std::size_t octave = 1; octave < 12; octave++)
  {
    r[octave] = r[0];
    constexpr_rotate(r[octave].rbegin(), r[octave].rbegin() + octave, r[octave].rend());
  }
  return r;
}

constexpr bool is_same(std::string_view lhs, std::string_view rhs)
{
  if(lhs.size() == rhs.size())
  {
    for(std::size_t i = 0; i < lhs.size(); i++)
    {
      if(lhs[i] != rhs[i])
        return false;
    }
    return true;
  }
  return false;
}

constexpr int get_scale(std::string_view s)
{
  using namespace std::literals;
  if(is_same(s, std::string_view("all")))
    return scale_type::all;
  else if(is_same(s, std::string_view("ionian")))
    return scale_type::ionian;
  else if(is_same(s, std::string_view("dorian")))
    return scale_type::dorian;
  else if(is_same(s, std::string_view("phyrgian")))
    return scale_type::phyrgian;
  else if(is_same(s, std::string_view("lydian")))
    return scale_type::lydian;
  else if(is_same(s, std::string_view("mixolydian")))
    return scale_type::mixolydian;
  else if(is_same(s, std::string_view("aeolian")))
    return scale_type::aeolian;
  else if(is_same(s, std::string_view("locrian")))
    return scale_type::locrian;
  else if(is_same(s, std::string_view("I")))
    return scale_type::I;
  else if(is_same(s, std::string_view("II")))
    return scale_type::II;
  else if(is_same(s, std::string_view("III")))
    return scale_type::III;
  else if(is_same(s, std::string_view("IV")))
    return scale_type::IV;
  else if(is_same(s, std::string_view("V")))
    return scale_type::V;
  else if(is_same(s, std::string_view("VI")))
    return scale_type::VI;
  else if(is_same(s, std::string_view("VII")))
    return scale_type::VII;
  else
    return scale_type::custom;
}

// clang-format off
static constexpr std::array<scales_array, scale_type::SCALES_MAX - 1> scales{
//                                      C     D     E  F     G     A     B
/* { scale::all,         */ make_scale({1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}) /* } */,
/* { scale::ionian,      */ make_scale({1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1}) /* } */,
/* { scale::dorian,      */ make_scale({1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0}) /* } */,
/* { scale::phyrgian,    */ make_scale({1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0}) /* } */,
/* { scale::lydian,      */ make_scale({1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1}) /* } */,
/* { scale::mixolydian,  */ make_scale({1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0}) /* } */,
/* { scale::aeolian,     */ make_scale({1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0}) /* } */,
/* { scale::locrian,     */ make_scale({1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0}) /* } */,
/* { scale::I,           */ make_scale({1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0}) /* } */,
/* { scale::II,          */ make_scale({0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0}) /* } */,
/* { scale::III,         */ make_scale({0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1}) /* } */,
/* { scale::IV,          */ make_scale({1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0}) /* } */,
/* { scale::V,           */ make_scale({0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1}) /* } */,
/* { scale::VI,          */ make_scale({1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0}) /* } */,
/* { scale::VII,         */ make_scale({0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1}) /* } */};
// clang-format on
static std::optional<std::size_t> find_closest_index(const scale_array& arr, std::size_t i)
{
  if(arr[i] == 1)
    return i;

  switch(i)
  {
    case 0:
      while(i != 12)
      {
        i++;
        if(arr[i] == 1)
          return i;
      }
      break;

    case 12:
      while(i != 0)
      {
        i--;
        if(arr[i] == 1)
          return i;
      }
      break;

    default: {
      std::size_t r = 0;
      while((i - r) != 0 && (i + r) != 12)
      {
        if(arr[i + r] == 1)
          return i + r;
        else if(arr[i - r] == 1)
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
  halp_meta(name, "Midi scale")
  halp_meta(c_name, "MidiScale")
  halp_meta(category, "Midi")
  halp_meta(author, "ossia score")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/midi-utilities.html#midi-scale")
  halp_meta(description, "Maps a midi input to a given scale")
  halp_meta(uuid, "06b33b83-bb67-4f7a-9980-f5d66e4266c5")

  struct
  {
    halp::midi_bus<"in", libremidi::message> midi;
    halp::string_enum_t<scale_type, "Scale"> sc; // FIXME check that this works
    octave_slider<"Base", 0, 1> base;
    octave_slider<"Transpose", -4, 4> transp;
  } inputs;
  struct
  {
    midi_out midi;
  } outputs;

  struct Note
  {
    uint8_t pitch{};
    uint8_t vel{};
    uint8_t chan{};
  };
  ossia::flat_map<uint8_t, Note> map;
  std::string scale{};
  int base{};
  int transpose{};

  void exec(const scale_array& scale, int transp)
  {
    auto& midi_in = inputs.midi;
    auto& midi_out = outputs.midi;
    for(const auto& msg : midi_in)
    {
      switch(msg.get_message_type())
      {
        case libremidi::message_type::NOTE_ON: {
          if(msg.bytes[1] >= 128)
            continue;

          // map to scale
          if(auto index = find_closest_index(scale, msg.bytes[1]))
          {
            // transpose
            auto res = msg;
            res.bytes[1] = (uint8_t)ossia::clamp(int(*index + transp), 0, 127);
            Note note{
                (uint8_t)res.bytes[1], (uint8_t)res.bytes[2],
                (uint8_t)res.get_channel()};
            auto it = this->map.find(msg.bytes[1]);
            if(it != this->map.end())
            {
              midi_out.note_off(res.get_channel(), it->second.pitch, res.bytes[2])
                  .timestamp
                  = 0;
              midi_out.push_back(res);
              midi_out.back().timestamp = 1; // FIXME does not work if last sample
              const_cast<Note&>(it->second) = note;
            }
            else
            {
              midi_out.push_back(res);
              midi_out.back().timestamp = 0;
              this->map.insert(std::make_pair((uint8_t)msg.bytes[1], note));
            }
          }
          break;
        }
        case libremidi::message_type::NOTE_OFF: {
          if(msg.bytes[1] >= 128)
            continue;

          auto it = this->map.find(msg.bytes[1]);
          if(it != this->map.end())
          {
            midi_out.note_off(msg.get_channel(), it->second.pitch, msg.bytes[2])
                .timestamp
                = 0;
            this->map.erase(it);
          }
          break;
        }
        default:
          midi_out.push_back(msg);
          break;
      }
    }
  }

  void update(const scale_array& scale, int transp)
  {
    auto& midi_out = outputs.midi;
    for(auto& notes : this->map)
    {
      Note& note = const_cast<Note&>(notes.second);
      if(auto index = find_closest_index(scale, notes.first))
      {
        if((*index + transp) != note.pitch)
        {
          midi_out.note_off(note.chan, note.pitch, note.vel).timestamp = 0;
          note.pitch = *index + transp;
          midi_out.note_on(note.chan, note.pitch, note.vel);
          midi_out.back().timestamp = 1; // FIXME does not work if last sample
        }
      }
    }
  }

  using tick = halp::tick_flicks;
  void operator()(const tick& tk)
  {
    const auto& new_scale = inputs.sc.value;
    const int new_base = inputs.base.value;
    const int new_transpose = inputs.transp.value;
    std::string_view scale{new_scale.data(), new_scale.size()};

    const auto new_scale_idx = get_scale(scale);

    auto apply = [&](auto f) {
      if(new_scale_idx >= 0 && new_scale_idx < scale_type::custom)
      {
        f(scales[new_scale_idx][new_base], new_transpose);
      }
      else
      {
        scale_array arr{{}};
        for(int oct = 0; oct < 10; oct++)
        {
          for(int i = 0; i < ossia::min(std::ssize(scale), 12); i++)
          {
            arr[oct * 12 + i] = (scale[i] == '1');
          }
        }
        f(arr, new_transpose);
      }
    };

#define forward_to_method(method_name)              \
  [&]<typename... Args>(Args&&... args) {           \
    this->method_name(std::forward<Args>(args)...); \
  }

    if(!this->map.empty()
       && (new_scale != this->scale || new_base != this->base
           || new_transpose != this->transpose))
    {
      apply(forward_to_method(update));
    }

    apply(forward_to_method(exec));
#undef forward_to_method

    this->scale = new_scale;
    this->base = new_base;
    this->transpose = new_transpose;
  }
};
}
