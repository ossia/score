#pragma once
#include <ossia/detail/string_map.hpp>

#include <Control/Widgets.hpp>

#include <array>
namespace Control
{
namespace Widgets
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

inline auto& waveformMap()
{
  static const ossia::string_view_map<Waveform> waveform_map{
      {"Sin", Sin},
      {"Triangle", Triangle},
      {"Saw", Saw},
      {"Square", Square},
      {"Sample & Hold", SampleAndHold},
      {"Noise 1", Noise1},
      {"Noise 2", Noise2},
      {"Noise 3", Noise3}};
  return waveform_map;
}
static constexpr auto WaveformChooser()
{
  return Control::make_enum(
      "Waveform",
      0U,
      ossia::make_array(
          "Sin", "Triangle", "Saw", "Square", "Sample & Hold", "Noise 1", "Noise 2", "Noise 3"),
      std::array<const char*, 16>{
          ":/icons/waves/sin_off.png",
          ":/icons/waves/sin_on.png",
          ":/icons/waves/triangle_off.png",
          ":/icons/waves/triangle_on.png",
          ":/icons/waves/saw_off.png",
          ":/icons/waves/saw_on.png",
          ":/icons/waves/square_off.png",
          ":/icons/waves/square_on.png",
          ":/icons/waves/sample_and_hold_off.png",
          ":/icons/waves/sample_and_hold_on.png",
          ":/icons/waves/noise1_off.png",
          ":/icons/waves/noise1_on.png",
          ":/icons/waves/noise2_off.png",
          ":/icons/waves/noise2_on.png",
          ":/icons/waves/noise3_off.png",
          ":/icons/waves/noise3_on.png"});
}

enum LoopMode
{
  Play,
  Record,
  Overdub,
  Stop
};
constexpr auto LoopChooser()
{
  return Control::make_enum("Loop", 0U, ossia::make_array("Play", "Record", "Overdub", "Stop"));
}
constexpr LoopMode GetLoopMode(std::string_view str) noexcept
{
  if (str == "Play")
    return LoopMode::Play;
  else if (str == "Record")
    return LoopMode::Record;
  else if (str == "Overdub")
    return LoopMode::Overdub;
  else if (str == "Stop")
    return LoopMode::Stop;
  return LoopMode::Stop;
}

constexpr auto QuantificationChooser()
{
  return Control::ComboBox<float, std::size(notes)>("Quantification", 2, notes);
}
constexpr auto ArpeggioChooser()
{
  return Control::ComboBox<int, std::size(arpeggios)>("Arpeggios", 0, arpeggios);
}

constexpr auto MusicalDurationChooser()
{
  return Control::ComboBox<float, std::size(nonnull_notes)>("Duration", 2, nonnull_notes);
}
constexpr auto DurationChooser()
{
  return Control::ComboBox<float, std::size(durations)>("Duration", 2, durations);
}
constexpr auto FreqSlider()
{
  return Control::LogFloatSlider("Frequency", 1.f, 20000.f, 200.f);
}
constexpr auto LFOFreqSlider()
{
  return Control::LogFloatSlider("Frequency", 0.01f, 100.f, 1.f);
}
constexpr auto FreqKnob()
{
  return Control::LogFloatKnob("Frequency", 1.f, 20000.f, 200.f);
}
constexpr auto LFOFreqKnob()
{
  return Control::LogFloatKnob("Frequency", 0.01f, 100.f, 1.f);
}
constexpr auto TimeSigChooser()
{
  return Control::TimeSignatureChooser("Sig.", "4/4");
}
constexpr auto TempoChooser()
{
  return Control::FloatSlider("Tempo", 20, 300, 120);
}

template <typename T>
constexpr auto MidiSpinbox(const T& name)
{
  return Control::IntSpinBox{name, 0, 127, 64};
}
template <typename T>
constexpr auto MidiChannel(const T& name)
{
  return Control::IntSpinBox{name, 1, 16, 1};
}
template <typename T>
constexpr auto MidiSlider(const T& name)
{
  return Control::IntSlider{name, 0, 127, 64};
}
template <typename T>
constexpr auto DefaultSlider(const T& name)
{
  return Control::FloatSlider{name, 0., 1., 0.5};
}

template <typename T>
constexpr auto OctaveSlider(const T& name, int neg_octaves, int octaves)
{
  return Control::IntSlider{name, 12 * neg_octaves, 12 * octaves, 0};
}
}
}
