#pragma once
#include <ossia/detail/enum_map.hpp>
#include <ossia/network/value/value.hpp>

#include <array>
namespace Control::Widgets
{
static const constexpr std::array<std::pair<const char*, float>, 14> durations{
    {{"Inf", -1.},
     {"Whole", 1.},
     {"Half", 1. / 2.},
     {"4th", 1. / 4.},
     {"8th", 1. / 8.},
     {"16th", 1. / 16.},
     {"32th", 1. / 32.},
     {"64th", 1. / 64.},
     {"Dotted Half", 3. / 4.},
     {"Dotted 4th", 3. / 8.},
     {"Dotted 8th", 3. / 16.},
     {"Dotted 16th", 3. / 32.},
     {"Dotted 32th", 3. / 64.},
     {"None", 0.}}};
static const constexpr std::array<std::pair<const char*, float>, 13> notes{
    {{"None", 0.},
     {"Whole", 1.},
     {"Half", 1. / 2.},
     {"4th", 1. / 4.},
     {"8th", 1. / 8.},
     {"16th", 1. / 16.},
     {"32th", 1. / 32.},
     {"64th", 1. / 64.},
     {"Dotted Half", 3. / 4.},
     {"Dotted 4th", 3. / 8.},
     {"Dotted 8th", 3. / 16.},
     {"Dotted 16th", 3. / 32.},
     {"Dotted 32th", 3. / 64.}}};

static const constexpr std::array<std::pair<const char*, float>, 12> nonnull_notes{
    {{"Whole", 1.},
     {"Half", 1. / 2.},
     {"4th", 1. / 4.},
     {"8th", 1. / 8.},
     {"16th", 1. / 16.},
     {"32th", 1. / 32.},
     {"64th", 1. / 64.},
     {"Dotted Half", 3. / 4.},
     {"Dotted 4th", 3. / 8.},
     {"Dotted 8th", 3. / 16.},
     {"Dotted 16th", 3. / 32.},
     {"Dotted 32th", 3. / 64.}}};

static const constexpr std::array<std::pair<const char*, int>, 5> arpeggios{
    {{"Forward", 0}, {"Backward", 1}, {"F->B", 2}, {"B->F", 3}, {"Chord", 4}}};

enum Waveform
{
  Sin,
  Triangle,
  Saw,
  Square,
  SampleAndHold,
  Noise1,
  Noise2,
  Noise3
};

#if 0
static constexpr auto WaveformChooser()
{
  return Control::make_enum(
      "Waveform", 0U,
      ossia::make_array(
          "Sin", "Triangle", "Saw", "Square", "Sample & Hold", "Noise 1", "Noise 2",
          "Noise 3"),
      std::array<const char*, 16>{
          ":/icons/wave_sin_off.png", ":/icons/wave_sin_on.png",
          ":/icons/wave_triangle_off.png", ":/icons/wave_triangle_on.png",
          ":/icons/wave_saw_off.png", ":/icons/wave_saw_on.png",
          ":/icons/wave_square_off.png", ":/icons/wave_square_on.png",
          ":/icons/wave_sample_and_hold_off.png", ":/icons/wave_sample_and_hold_on.png",
          ":/icons/wave_noise1_off.png", ":/icons/wave_noise1_on.png",
          ":/icons/wave_noise2_off.png", ":/icons/wave_noise2_on.png",
          ":/icons/wave_noise3_off.png", ":/icons/wave_noise3_on.png"});
}

#endif
}

#include <ossia/detail/pod_vector.hpp>
#include <ossia/detail/small_vector.hpp>
#include <ossia/detail/variant.hpp>

#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/midi.hpp>
#include <halp/polyfill.hpp>
#include <halp/static_string.hpp>
#include <libremidi/message.hpp>

namespace Nodes
{
static constexpr auto multichannel_max_count = 8;
using multichannel_output_vector = ossia::small_pod_vector<float, multichannel_max_count>;
using multichannel_output_type = ossia::variant<float, multichannel_output_vector>;
using midi_out = halp::midi_out_bus<"out", libremidi::message>;
using value_output_callback = halp::basic_callback<void(ossia::value)>;
using value_out = halp::callback<"out", ossia::value>;
template <halp::static_string Str>
using midi_spinbox = halp::spinbox_i32<Str, halp::irange{0, 127, 64}>;
template <halp::static_string Str>
using midi_channel = halp::spinbox_i32<Str, halp::irange{1, 16, 1}>;
template <halp::static_string Str>
using midi_slider = halp::hslider_i32<Str, halp::irange{0, 127, 64}>;
template <halp::static_string Str>
using default_slider = halp::hslider_f32<Str>;
template <halp::static_string Str, int neg_oct, int pos_oct>
using octave_slider
    = halp::hslider_i32<Str, halp::irange{12 * neg_oct, 12 * pos_oct, 0}>;

template <halp::static_string lit, typename T, bool event = true>
struct ossia_port
{
  halp_meta(is_event, event)
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }

  T* value{};
};

template <halp::static_string lit, auto* array, int init_idx>
struct combobox_from_array
{
  static clang_buggy_consteval auto name() { return std::string_view{lit.value}; }
  enum widget
  {
    combobox
  };
  struct range
  {
    decltype(Control::Widgets::notes) values = Control::Widgets::notes;
    float init = 1. / 4.;
  };
  float value = 1. / 4.;
};

template <halp::static_string lit>
using quant_selector = combobox_from_array<lit, &Control::Widgets::notes, 2>;

template <halp::static_string lit>
using duration_selector = combobox_from_array<lit, &Control::Widgets::durations, 2>;
template <halp::static_string lit>
using musical_duration_selector
    = combobox_from_array<lit, &Control::Widgets::nonnull_notes, 2>;
}
