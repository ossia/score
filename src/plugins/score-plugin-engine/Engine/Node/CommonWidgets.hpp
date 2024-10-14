#pragma once
#include <ossia/detail/enum_map.hpp>

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
