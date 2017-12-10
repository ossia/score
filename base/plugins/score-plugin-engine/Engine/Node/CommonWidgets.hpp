#pragma once
#include <array>
#include <Engine/Node/Widgets.hpp>
#include <ossia/detail/string_map.hpp>
namespace Process
{
namespace Widgets
{



static const constexpr std::array<std::pair<const char*, float>, 14> durations
{{
    {"None",  0.},
    {"Inf",  -1.},
    {"Whole", 1.},
    {"Half",  1./2.},
    {"4th",   1./4.},
    {"8th",   1./8.},
    {"16th",  1./16.},
    {"32th",  1./32.},
    {"64th",  1./64.},
    {"Dotted Half",  3./4.},
    {"Dotted 4th",   3./8.},
    {"Dotted 8th",   3./16.},
    {"Dotted 16th",  3./32.},
    {"Dotted 32th",  3./64.}
}};
static const constexpr std::array<std::pair<const char*, float>, 13> notes
{{
    {"None",  0.},
    {"Whole", 1.},
    {"Half",  1./2.},
    {"4th",   1./4.},
    {"8th",   1./8.},
    {"16th",  1./16.},
    {"32th",  1./32.},
    {"64th",  1./64.},
    {"Dotted Half",  3./4.},
    {"Dotted 4th",   3./8.},
    {"Dotted 8th",   3./16.},
    {"Dotted 16th",  3./32.},
    {"Dotted 32th",  3./64.}
}};

static const constexpr std::array<std::pair<const char*, float>, 12> nonnull_notes
{{
    {"Whole", 1.},
    {"Half",  1./2.},
    {"4th",   1./4.},
    {"8th",   1./8.},
    {"16th",  1./16.},
    {"32th",  1./32.},
    {"64th",  1./64.},
    {"Dotted Half",  3./4.},
    {"Dotted 4th",   3./8.},
    {"Dotted 8th",   3./16.},
    {"Dotted 16th",  3./32.},
    {"Dotted 32th",  3./64.}
}};

enum Waveform
{
  Sin, Triangle, Saw, Square, Noise1, Noise2, Noise3
};
inline
auto& waveformMap()
{
    static const ossia::string_view_map<Waveform> waveform_map{
      {"Sin", Sin}, {"Triangle", Triangle}, {"Saw", Saw},
      {"Square", Square},
      {"Noise 1", Noise1}, {"Noise 2", Noise2}, {"Noise 3", Noise3} };
    return waveform_map;
}
static constexpr auto WaveformChooser()
{
    return Process::make_enum(
                "Waveform", 0U,
                Process::array("Sin", "Triangle", "Saw", "Square", "Noise 1", "Noise 2", "Noise 3")
                );
}
constexpr auto QuantificationChooser()
{ return Process::ComboBox<float, std::size(notes)>("Quant.", 2, notes); }

constexpr auto MusicalDurationChooser()
{ return Process::ComboBox<float, std::size(nonnull_notes)>("Dur.", 2, nonnull_notes); }
constexpr auto DurationChooser()
{ return Process::ComboBox<float, std::size(durations)>("Dur.", 2, durations); }
constexpr auto FreqChooser()
{ return Process::LogFloatSlider("Freq.", 1.f, 20000.f, 200.f); }
constexpr auto LFOFreqChooser()
{ return Process::LogFloatSlider("Freq.", 0.01f, 100.f, 1.f); }


template<typename T>
constexpr auto MidiSpinbox(const T& name)
{
    return Process::IntSpinBox{name, 0, 127, 64};
}
template<typename T>
constexpr auto MidiChannel(const T& name)
{
    return Process::IntSpinBox{name, 1, 16, 1};
}
template<typename T>
constexpr auto MidiSlider(const T& name)
{
    return Process::IntSlider{name, 0, 127, 64};
}
template<typename T>
constexpr auto DefaultSlider(const T& name)
{
    return Process::FloatSlider{name, 0., 1., 0.5};
}

template<typename T>
constexpr auto OctaveSlider(const T& name, int neg_octaves, int octaves)
{
    return Process::IntSlider{name, 12 * neg_octaves, 12 * octaves, 0};
}
}

}
