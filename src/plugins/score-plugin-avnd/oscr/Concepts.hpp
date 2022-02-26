#pragma once

#include <Control/Widgets.hpp>
#include <ossia/dataflow/audio_port.hpp>
#include <ossia/dataflow/safe_nodes/tick_policies.hpp>
#include <boost/container/vector.hpp>
#include <Process/ProcessFlags.hpp>
#include <Process/ProcessMetadata.hpp>
#include <score/plugins/UuidKey.hpp>

#include <cmath>
#include <gsl/span>
#include <type_traits>
#if __has_include(<concepts>)
#include <concepts>
#endif

namespace oscr
{

#define make_uuid(text) score::uuids::string_generator::compute((text))
#if defined(_MSC_VER)
#define uuid_constexpr inline
#else
#define uuid_constexpr constexpr
#endif

#define constant static inline constexpr auto

#if __cpp_lib_concepts >= 202002L
template<typename T, typename U>
concept convertible_to = std::convertible_to<T, U>;
#else
template<typename _From, typename _To>
concept convertible_to = std::is_convertible_v<_From, _To>;
#endif

#if __has_include(<concepts>)
template<typename T, typename U>
concept same_as = std::same_as<T, U>;
#else
template<typename T, typename U>
concept same_as = std::is_same_v<T, U> && std::is_same_v<U, T>;
#endif


template<typename N>
consteval score::uuid_t uuid_from_string() {
  if constexpr(requires { { N::uuid() } -> convertible_to<score::uuid_t>; })
  {
    return N::uuid();
  }
  else
  {
    constexpr const char* str = N::uuid();
    return score::uuids::string_generator::compute(str, str + 37);
  }
}

template<typename T>
concept NamedPort = requires(T a) {
  { QString::fromUtf8(a.name()) } ;
};

//// Audio inputs ////
template<typename T>
concept MultichannelAudioInput = requires (T a) {
  { a.samples.channels() } -> convertible_to<std::size_t>;
  { a.samples[0].size() } -> convertible_to<std::size_t>;
  { a.samples[0][0] } -> same_as<const double&>;
};
template<typename T>
concept MultichannelAudioOutput = requires (T a) {
  { a.samples.channels() } -> convertible_to<std::size_t>;
  { a.samples[0].size() } -> convertible_to<std::size_t>;
  { a.samples[0][0] } -> same_as<double&>;
};

template<typename T>
concept AudioEffectInput = requires (T a) {
  { a.samples } -> convertible_to<const double**>;
  { a.channels } -> convertible_to<std::size_t>;
};
template<typename T>
concept AudioEffectOutput = requires (T a) {
  { a.samples } -> convertible_to<double**>;
  //{ a.channels } -> convertible_to<std::size_t>;
};

template<typename T>
concept PortAudioInput = requires (T a) {
  { (a.port) } -> same_as<const ossia::audio_port*&>;
};

template<typename T>
concept PortAudioOutput = requires (T a) {
  { (a.port) } -> same_as<ossia::audio_port*&>;
};

template<typename T>
concept AudioInput = NamedPort<T> && (MultichannelAudioInput<T> || AudioEffectInput<T> || PortAudioInput<T>);
template<typename T>
concept AudioOutput = NamedPort<T> && (MultichannelAudioOutput<T> || AudioEffectOutput<T> || PortAudioOutput<T>);

//// Control ////
template<typename T>
concept ControlInput = requires (T a) {
  { a.control() };
};
template<typename T>
concept FullControlImpl = requires (T a) {
  { a.control() } -> convertible_to<::ossia::safe_nodes::control_in>;
};


template<ControlInput T>
constexpr auto get_control()
{
  if constexpr(FullControlImpl<T>)
  {
    return T::control();
  }
  else
  {
    using value_type = decltype(T::value);
    constexpr auto c = T::control();
    constexpr auto name = T::name();
    if constexpr(std::is_same_v<value_type, float>) {
      return Control::FloatSlider{name, c.min, c.max, c.init};
    }
    else if constexpr(std::is_same_v<value_type, int>) {
      return Control::IntSlider{name, c.min, c.max, c.init};
    }
    else if constexpr(std::is_same_v<value_type, bool>) {
      return Control::Toggle{name, c.init};
    }
    else if constexpr(std::is_same_v<value_type, std::string>) {
      return Control::LineEdit{name, c.init};
    }
  }
}
template<ControlInput T>
constexpr auto get_control(const T& t)
{ return get_control<T>(); }

template<typename T>
concept ControlOutput = requires (T a) {
  { a.display() };
};

//// Value ////
template<typename T>
concept PortValueInput = (!ControlInput<T>) && requires (T a) {
  { (a.port) } -> same_as<const ossia::value_port*&>;
};

template<typename T>
concept PortValueOutput = (!ControlOutput<T>) && requires (T a) {
  { (a.port) } -> same_as<ossia::value_port*&>;
};

template<typename T>
concept TimedValueInput = (!ControlInput<T>) && requires (T a) {
  { a.values[0] } -> convertible_to<ossia::value>;
};

template<typename T>
concept TimedValueOutput = (!ControlOutput<T>) && requires (T a) {
  { a.values[0] } -> convertible_to<ossia::value>;
};

template<typename T>
concept SingleValueInput = (!ControlInput<T>) && requires (T a) {
  { a.value } -> convertible_to<ossia::value>;
};

template<typename T>
concept SingleValueOutput = (!ControlOutput<T>) && requires (T a) {
  { a.value } -> convertible_to<ossia::value>;
};

template<typename T>
concept ValueInput = NamedPort<T> && (PortValueInput<T> || TimedValueInput<T> || SingleValueInput<T>);

template<typename T>
concept ValueOutput = NamedPort<T> && (PortValueOutput<T> || TimedValueOutput<T> || SingleValueOutput<T>);


//// Texture ////
template<typename T>
concept CPUTextureInput = requires (T a) {
  { a.texture.bytes } ;
};

template<typename T>
concept TextureInput = NamedPort<T> && CPUTextureInput<T>;

template<typename T>
concept CPUTextureOutput = requires (T a) {
  { a.texture.bytes } ;
  { a.texture.width } ;
  { a.texture.height } ;
  { a.texture.changed } ;
};

template<typename T>
concept TextureOutput = NamedPort<T> && CPUTextureOutput<T>;


//// MIDI ////

template<typename T>
concept PortMidiInput = requires (T a) {
  { (a.port) } -> same_as<const ossia::midi_port*&>;
};

template<typename T>
concept PortMidiOutput = requires (T a) {
  { (a.port) } -> same_as<ossia::midi_port*&>;
};

template<typename T>
concept MessagesMidiInput = requires (T a) {
  { a.messages[0] } -> convertible_to<const libremidi::message&>;
};

template<typename T>
concept MessagesMidiOutput = requires (T a) {
  { a.messages[0] } -> convertible_to<libremidi::message&>;
};

template<typename T>
concept MidiInput = NamedPort<T> && (PortMidiInput<T> || MessagesMidiInput<T>);
template<typename T>
concept MidiOutput = NamedPort<T> &&  (PortMidiOutput<T> || MessagesMidiOutput<T>);

template<typename T>
concept TimedVec = requires (T a) {
  { a.begin()->first } -> convertible_to<int64_t>;
  { a.begin()->second };
};



template<typename T>
concept Inputs = std::is_aggregate_v<decltype(T::inputs)> && !std::is_empty_v<decltype(T::inputs)>;
template<typename T>
concept Outputs = std::is_aggregate_v<decltype(T::outputs)> && !std::is_empty_v<decltype(T::outputs)>;

template<typename T>
concept RunnableWithoutArguments = requires(T t)
{
  { t() };
};
template<typename T>
concept RunnableWithSampleCount = requires(T t)
{
  { t(std::size_t{}) };
};
template<typename T>
concept RunnableWithTokenRequest = requires(T t)
{
  { t(ossia::token_request{}, ossia::exec_state_facade{}) };
};

template<typename T>
concept Runnable = RunnableWithSampleCount<T> || RunnableWithTokenRequest<T> || RunnableWithoutArguments<T>;


template<typename T>
concept DataflowNode = Runnable<T> && requires(T t)
{
  { t.name() };
  { t.uuid() };
};


template<typename T>
using IsAudioInput = typename std::integral_constant< bool, AudioInput<T>>;
template<typename T>
using IsAudioOutput = typename std::integral_constant< bool, AudioOutput<T>>;

template<typename T>
using IsValueInput = typename std::integral_constant< bool, ValueInput<T>>;
template<typename T>
using IsValueOutput = typename std::integral_constant< bool, ValueOutput<T>>;

template<typename T>
using IsTextureInput = typename std::integral_constant< bool, TextureInput<T>>;
template<typename T>
using IsTextureOutput = typename std::integral_constant< bool, TextureOutput<T>>;

template<typename T>
using IsMidiInput = typename std::integral_constant< bool, MidiInput<T>>;
template<typename T>
using IsMidiOutput = typename std::integral_constant< bool, MidiOutput<T>>;

template<typename T>
using IsControlInput = typename std::integral_constant< bool, ControlInput<T>>;
template<typename T>
using IsControlOutput = typename std::integral_constant< bool, ControlOutput<T>>;

struct multichannel_audio_view {
  ossia::audio_vector* buffer{};
  int64_t offset{};
  int64_t duration{};

  gsl::span<const double> operator[](std::size_t i) const noexcept
  {
    auto& chan = (*buffer)[i];
    int64_t min_dur = std::min(int64_t(chan.size()) - offset, duration);
    if(min_dur < 0)
      min_dur = 0;

    return gsl::span<const double>{chan.data() + offset, std::size_t(min_dur)};
  }

  std::size_t channels() const noexcept { return buffer->size(); }
  void resize(std::size_t i) const noexcept { return buffer->resize(i); }
  void reserve(std::size_t channels, std::size_t bufferSize)
  {
    resize(channels);
    for(auto& vec : *buffer) vec.reserve(bufferSize);
  }
};

struct multichannel_audio {
  ossia::audio_vector* buffer{};
  int64_t offset{};
  int64_t duration{};

  gsl::span<double> operator[](std::size_t i) const noexcept
  {
    auto& chan = (*buffer)[i];
    int64_t min_dur = std::min(int64_t(chan.size()) - offset, duration);
    if(min_dur < 0)
      min_dur = 0;

    return gsl::span<double>{chan.data() + offset, std::size_t(min_dur)};
  }

  std::size_t channels() const noexcept { return buffer->size(); }
  void resize(std::size_t channels, std::size_t samples_to_write) const noexcept
  {
    buffer->resize(channels);
    for(auto& c : *buffer)
      c.resize(offset + samples_to_write);
  }

  void reserve(std::size_t channels, std::size_t bufferSize)
  {
    buffer->resize(channels);
    for(auto& c : *buffer)
      c.reserve(bufferSize);
  }
};


struct rgba_texture {
  unsigned char* bytes{};
  int width{};
  int height{};
  bool changed{};

  static auto allocate(int width, int height)
  {
    using namespace boost::container;
    return vector<unsigned char>(width * height * 4, default_init);
  }

  void update(unsigned char* data, int w, int h)
  { bytes = data; width = w; height = h; changed = true; }
};

}



#include <boost/pfr.hpp>
namespace boost::pfr
{

template <class T, class F>
void for_each_field_ref(T&& value, F&& func) {
  constexpr std::size_t fields_count_val = boost::pfr::detail::fields_count<std::remove_reference_t<T>>();

  ::boost::pfr::detail::for_each_field_dispatcher(
      value,
      [&func](auto&& t) mutable {
        // MSVC related workaround. Its lambdas do not capture constexprs.
        constexpr std::size_t fields_count_val_in_lambda
            = boost::pfr::detail::fields_count<std::remove_reference_t<T>>();

        ::boost::pfr::detail::for_each_field_impl(
            t,
            std::forward<F>(func),
            detail::make_index_sequence<fields_count_val_in_lambda>{},
            std::is_rvalue_reference<T&&>{} );
      },
      detail::make_index_sequence<fields_count_val>{} );
}
}
