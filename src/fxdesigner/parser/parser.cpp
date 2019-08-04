#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <array>
#include <vector>
#include <experimental/type_traits>
#include <cxxabi.h>
template <class T>
using has_values_t = decltype(std::declval<T>().values);
namespace ossia
{
template <typename... Args>
constexpr std::array<const char*, sizeof...(Args)>
make_array(Args&&... args) noexcept
{
  return {args...};
}
}

QString demangle(const char* mangled)
{
  std::string s;
  s.resize(1024);
  std::size_t sz = 1024;
  int status;
  char *realname = abi::__cxa_demangle(mangled, s.data(), &sz, &status);

  return QString(realname).remove("Control::");
}

template <class F, class... Ts, std::size_t... Is>
void for_each_in_tuple(
    const std::tuple<Ts...>& tuple, F&& func, std::index_sequence<Is...>)
{
  (std::forward<F>(func)(std::get<Is>(tuple)), ...);
}

template <class F, class... Ts>
void for_each_in_tuple(const std::tuple<Ts...>& tuple, F&& func)
{
  for_each_in_tuple(
      tuple, std::forward<F>(func), std::make_index_sequence<sizeof...(Ts)>());
}

template <class F>
void for_each_in_tuple(const std::tuple<>& tuple, const F& func)
{
}

namespace Control
{
struct Meta_base {

  struct value_in { const char* str; };
  struct value_out { const char* str; };
  struct midi_in { const char* str; };
  struct midi_out { const char* str; };
  struct audio_in { const char* str; };
  struct audio_out { const char* str; };
  struct address_in { const char* str; };

  static inline const constexpr std::array<value_in, 0> value_ins{};
  static inline const constexpr std::array<value_out, 0> value_outs{};
  static inline const constexpr std::array<audio_in, 0> audio_ins{};
  static inline const constexpr std::array<audio_out, 0> audio_outs{};
  static inline const constexpr std::array<midi_in, 0> midi_ins{};
  static inline const constexpr std::array<midi_out, 0> midi_outs{};
  static inline const constexpr std::array<address_in, 0> address_ins{};
  static inline const constexpr std::tuple<> controls{};

};


struct LineEdit { template<typename... Args> constexpr LineEdit(const char* n, Args&&...): name{n} { } const char* name; };
struct FloatSlider { template<typename... Args> constexpr FloatSlider(const char* n, Args&&...): name{n} { } const char* name; };
struct FloatKnob { template<typename... Args> constexpr FloatKnob(const char* n, Args&&...): name{n} { } const char* name; };
struct LogFloatSlider { template<typename... Args> constexpr LogFloatSlider(const char* n, Args&&...): name{n} { } const char* name; };
struct LogFloatKnob { template<typename... Args> constexpr LogFloatKnob(const char* n, Args&&...): name{n} { } const char* name; };
struct IntKnob { template<typename... Args> constexpr IntKnob(const char* n, Args&&...): name{n} { } const char* name; };
struct IntSlider { template<typename... Args> constexpr IntSlider(const char* n, Args&&...): name{n} { } const char* name; };
struct IntSpinBox{ template<typename... Args> constexpr IntSpinBox(const char* n, Args&&...): name{n} { } const char* name; };
struct Toggle{ template<typename... Args> constexpr Toggle(const char* n, Args&&...): name{n} { } const char* name; };
struct TimeSignatureChooser{ template<typename... Args> constexpr TimeSignatureChooser(const char* n, Args&&...): name{n} { } const char* name; };
struct RGBAEdit{ template<typename... Args> constexpr RGBAEdit(const char* n, Args&&...): name{n} { } const char* name; };
struct XYZEdit{ template<typename... Args> constexpr XYZEdit(const char* n, Args&&...): name{n} { } const char* name; };
template<std::size_t N>
struct Enum{ template<typename... Args> constexpr Enum(const char* n, std::size_t s, std::array<const char*, N> values): name{n}, values{values} { } const char* name; std::array<const char*, N> values; };
template<typename T, int N>
struct ComboBox{ template<typename... Args> constexpr ComboBox(const char* n, int, std::array<const char*, N> values): name{n}, values{values} { } const char* name; std::array<const char*, N> values; };

template<std::size_t N>
constexpr auto make_enum(const char* n, std::size_t s, std::array<const char*, N> values)
{
  return Enum<N>{n, s, values};
}
namespace Widgets
{
static constexpr const std::array<const char *, 1> notes{};
static constexpr const std::array<const char *, 1> nonnull_notes{};
static constexpr const std::array<const char *, 1> durations{};
static constexpr auto WaveformChooser()
{
  return Control::make_enum(
      "Waveform",
      0U,
      ossia::make_array(
          "Sin",
          "Triangle",
          "Saw",
          "Square",
          "Sample & Hold",
          "Noise 1",
          "Noise 2",
          "Noise 3"));
}
constexpr auto QuantificationChooser()
{
  return Control::ComboBox<float, std::size(notes)>(
      "Quantification", 2, notes);
}

constexpr auto MusicalDurationChooser()
{
  return Control::ComboBox<float, std::size(nonnull_notes)>(
      "Duration", 2, nonnull_notes);
}
constexpr auto DurationChooser()
{
  return Control::ComboBox<float, std::size(durations)>(
      "Duration", 2, durations);
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
namespace Process
{
enum ProcessCategory
{

  Other,
  Automation,
  Generator,   // lfo, etc
  MediaSource, // sound, video, image, etc
  Analyzer,
  AudioEffect, // audio in and audio out
  MidiEffect,  // midi in and midi out
  Synth,       // midi in and audio out
  Mapping,     // value in and value out
  Script,      // JS, PD, etc
  Structure    // scenario, loop, etc
};
}
constexpr int make_uuid(const char*) { return 0; }

struct MetaParser
{
  MetaParser()
  {

  }


  template<typename T>
  void parse(T meta)
  {
    QJsonObject obj;
    obj["prettyName"] = meta.prettyName;

    QJsonArray value_ins;  for(auto v : T::value_ins)  value_ins.push_back(v.str);
    QJsonArray value_outs; for(auto v : T::value_outs) value_outs.push_back(v.str);
    QJsonArray audio_ins;  for(auto v : T::audio_ins)  audio_ins.push_back(v.str);
    QJsonArray audio_outs; for(auto v : T::audio_outs) audio_outs.push_back(v.str);
    QJsonArray midi_ins;   for(auto v : T::midi_ins)   midi_ins.push_back(v.str);
    QJsonArray midi_outs;  for(auto v : T::midi_outs)  midi_outs.push_back(v.str);

    QJsonArray controls;
    for_each_in_tuple(T::controls, [&] (auto control) {
      QJsonObject ctrl;
      ctrl["Type"] = demangle(typeid(control).name());
      ctrl["Name"] = control.name;

      if constexpr(std::experimental::is_detected_v<has_values_t, decltype(control)>)
      {
        QJsonArray v;
        for(auto val : control.values)
          v.push_back(val);
        ctrl["Values"] = v;
      }
      controls.push_back(ctrl);
    });
    obj["ValueIns"] = value_ins;
    obj["ValueOuts"] = value_outs;
    obj["MidiIns"] = midi_ins;
    obj["MidiOuts"] = midi_outs;
    obj["AudioIns"] = audio_ins;
    obj["AudioOuts"] = audio_outs;
    obj["Controls"] = controls;


    std::cout << QJsonDocument(obj).toJson().toStdString() << std::endl;
  }
};

$$METADATA$$

int main()
{
  MetaParser m;
  m.parse(Metadata{});
}
