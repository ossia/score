#pragma once
#include <Fx/Types.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/math.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>

#include <array>
#include <optional>
#include <vector>

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
static constexpr void constexpr_swap(T& a, T& b)
{
  T tmp = a;
  a = b;
  b = tmp;
}

template <typename Iterator>
static constexpr void constexpr_rotate(Iterator first, Iterator middle, Iterator last)
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
static constexpr scales_array make_scale(std::initializer_list<bool> notes)
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

static constexpr bool is_same(std::string_view lhs, std::string_view rhs)
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

static constexpr int get_scale(std::string_view s)
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
//! The scale note closest to `i`, searching upwards first; none for an empty scale.
static std::optional<std::size_t> find_closest_index(const scale_array& arr, std::size_t i)
{
  const int n = int(arr.size());
  const int note = int(i);
  for(int r = 0; r < n; r++)
  {
    if(note + r < n && arr[note + r])
      return note + r;
    if(note - r >= 0 && arr[note - r])
      return note - r;
  }
  return std::nullopt;
}

//! What the twelve degree controls do: nothing (the scale applies), or
//! move each degree by its own number of semitones.
enum class scale_mode : uint8_t
{
  scale,
  remap
};

//! Where a note out of the scale goes.
enum class scale_direction : uint8_t
{
  nearest,
  up,
  down
};

//! The scale note in `dir` from `i`, or in the other direction when there is
//! none that way; none for an empty scale.
static std::optional<std::size_t>
find_index(const scale_array& arr, std::size_t i, scale_direction dir)
{
  if(dir == scale_direction::nearest)
    return find_closest_index(arr, i);
  const int n = int(arr.size());
  const int step = dir == scale_direction::up ? 1 : -1;
  for(int k = int(i); k >= 0 && k < n; k += step)
    if(arr[k])
      return k;
  for(int k = int(i) - step; k >= 0 && k < n; k -= step)
    if(arr[k])
      return k;
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

    struct : halp::enum_t<scale_mode, "Mode">
    {
      halp_meta(
          description,
          "Scale moves notes into the scale; Remap moves each degree by the "
          "semitones set for it")
      struct range
      {
        std::string_view values[2] = {"Scale", "Remap"};
        scale_mode init = scale_mode::scale;
      };
    } mode;

    struct : halp::enum_t<scale_direction, "Direction">
    {
      halp_meta(
          description,
          "Where a note out of the scale goes: the nearest scale note, the next "
          "one up, or the next one down")
      struct range
      {
        std::string_view values[3] = {"Nearest", "Up", "Down"};
        scale_direction init = scale_direction::nearest;
      };
    } direction;

    // Remap: semitones added to each degree, counted from Base.
    halp::spinbox_i32<"I", halp::irange{-12, 12, 0}> d0;
    halp::spinbox_i32<"I#", halp::irange{-12, 12, 0}> d1;
    halp::spinbox_i32<"II", halp::irange{-12, 12, 0}> d2;
    halp::spinbox_i32<"II#", halp::irange{-12, 12, 0}> d3;
    halp::spinbox_i32<"III", halp::irange{-12, 12, 0}> d4;
    halp::spinbox_i32<"IV", halp::irange{-12, 12, 0}> d5;
    halp::spinbox_i32<"IV#", halp::irange{-12, 12, 0}> d6;
    halp::spinbox_i32<"V", halp::irange{-12, 12, 0}> d7;
    halp::spinbox_i32<"V#", halp::irange{-12, 12, 0}> d8;
    halp::spinbox_i32<"VI", halp::irange{-12, 12, 0}> d9;
    halp::spinbox_i32<"VI#", halp::irange{-12, 12, 0}> d10;
    halp::spinbox_i32<"VII", halp::irange{-12, 12, 0}> d11;
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
  //! Held input keys (see key()) and the output note each plays.
  ossia::flat_map<uint16_t, Note> map;
  //! Held keys per output note: two keys can map to the same scale note, which
  //! must only stop with the last one.
  std::array<std::array<uint8_t, 128>, 17> playing{};
  std::string scale{};
  int base{};
  int transpose{};
  scale_mode mode{};
  scale_direction direction{};
  std::array<int, 12> offsets{};

  static uint16_t key(int chan, int pitch) noexcept { return uint16_t(chan * 128 + pitch); }

  //! The map never allocates on the audio thread.
  Node() { map.reserve(16 * 128); }

  //! Uses the mode, direction, offsets and base of the current tick.
  std::optional<uint8_t>
  map_note(const scale_array& scale, int pitch, int transp) const noexcept
  {
    if(mode == scale_mode::remap)
    {
      const int degree = ((pitch - base) % 12 + 12) % 12;
      return (uint8_t)ossia::clamp(pitch + offsets[degree] + transp, 0, 127);
    }
    if(auto index = find_index(scale, pitch, direction))
      return (uint8_t)ossia::clamp(int(*index) + transp, 0, 127);
    return std::nullopt;
  }

  void start(int chan, int pitch, int vel, int64_t ts)
  {
    if(playing[chan][pitch]++ == 0)
      outputs.midi.note_on(chan, pitch, vel).timestamp = ts;
  }

  void stop(int chan, int pitch, int vel, int64_t ts)
  {
    auto& count = playing[chan][pitch];
    if(count == 0)
      return;
    if(--count == 0)
      outputs.midi.note_off(chan, pitch, vel).timestamp = ts;
  }

  void exec(const scale_array& scale, int transp)
  {
    for(const auto& msg : inputs.midi)
    {
      const auto type = msg.get_message_type();
      const bool is_note
          = type == libremidi::message_type::NOTE_ON
            || type == libremidi::message_type::NOTE_OFF;
      if(!is_note || msg.size() < 3)
      {
        if(type == libremidi::message_type::CONTROL_CHANGE && msg.size() >= 3
           && (msg.bytes[1] == 120 || msg.bytes[1] == 123))
          release_channel(msg.get_channel());
        outputs.midi.push_back(msg);
        continue;
      }

      const int chan = msg.get_channel();
      const int pitch = msg.bytes[1] & 0x7F;
      const int vel = msg.bytes[2] & 0x7F;
      const auto ts = msg.timestamp;
      const auto k = key(chan, pitch);

      // A held key stops its note on re-press or release (note on with
      // velocity 0 is a release).
      if(auto it = map.find(k); it != map.end())
      {
        stop(it->second.chan, it->second.pitch, vel, ts);
        map.erase(it);
      }

      if(type == libremidi::message_type::NOTE_ON && vel > 0)
      {
        if(auto out = map_note(scale, pitch, transp))
        {
          start(chan, *out, vel, ts);
          map.insert({k, Note{*out, (uint8_t)vel, (uint8_t)chan}});
        }
      }
    }
  }

  //! Moves the held keys to their notes in the new scale / transposition.
  void update(const scale_array& scale, int transp)
  {
    for(auto it = map.begin(); it != map.end();)
    {
      Note& note = it->second;
      const auto out = map_note(scale, it->first % 128, transp);
      if(out && *out == note.pitch)
      {
        ++it;
        continue;
      }

      stop(note.chan, note.pitch, 0, 0);
      if(out)
      {
        start(note.chan, *out, note.vel, 0);
        note.pitch = *out;
        ++it;
      }
      else
      {
        it = map.erase(it);
      }
    }
  }

  //! On all notes / sound off: forget the channel's held keys, or a later
  //! scale change would restart their notes.
  void release_channel(int chan)
  {
    for(auto it = map.begin(); it != map.end();)
    {
      if(it->second.chan == chan)
        it = map.erase(it);
      else
        ++it;
    }
    playing[chan] = {};
  }

  using tick = halp::tick_flicks;
  void operator()(const tick& tk)
  {
    const auto& new_scale = inputs.sc.value;
    // A base of 12 is the same scale one octave up.
    const int new_base = ((inputs.base.value % 12) + 12) % 12;
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
        const auto degrees = ossia::min(std::ssize(scale), std::ptrdiff_t(12));
        for(std::size_t note = 0; note < arr.size(); note++)
        {
          const auto degree = std::ptrdiff_t(note % 12);
          arr[note] = degree < degrees && scale[degree] == '1';
        }
        f(arr, new_transpose);
      }
    };

#define forward_to_method(method_name)              \
  [&]<typename... Args>(Args&&... args) {           \
    this->method_name(std::forward<Args>(args)...); \
  }

    const auto& in = inputs;
    const std::array<int, 12> new_offsets{
        in.d0.value, in.d1.value, in.d2.value, in.d3.value, in.d4.value,  in.d5.value,
        in.d6.value, in.d7.value, in.d8.value, in.d9.value, in.d10.value, in.d11.value};
    const bool changed = new_scale != this->scale || new_base != this->base
                         || new_transpose != this->transpose
                         || in.mode.value != this->mode
                         || in.direction.value != this->direction
                         || new_offsets != this->offsets;

    this->base = new_base;
    this->mode = in.mode.value;
    this->direction = in.direction.value;
    this->offsets = new_offsets;

    if(!this->map.empty() && changed)
    {
      apply(forward_to_method(update));
    }

    apply(forward_to_method(exec));
#undef forward_to_method

    if(new_scale != this->scale)
      this->scale = new_scale;
    this->transpose = new_transpose;
  }
};
}
