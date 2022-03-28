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
#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/midi_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/concepts/channels.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>
#include <avnd/concepts/gfx.hpp>

namespace oscr
{


#define make_uuid(text) score::uuids::string_generator::compute((text))
#if defined(_MSC_VER)
#define uuid_constexpr inline
#else
#define uuid_constexpr constexpr
#endif

#define constant static inline constexpr auto



template<typename N>
consteval score::uuid_t uuid_from_string() {
  if constexpr(requires { { N::uuid() } -> std::convertible_to<score::uuid_t>; })
  {
    return N::uuid();
  }
  else
  {
    constexpr const char* str = N::uuid();
    return score::uuids::string_generator::compute(str, str + 37);
  }
}

}


namespace oscr
{

template<typename T, std::size_t N>
consteval auto to_const_char_array(const T(& val)[N])
{
  //using pair_type = typename std::decay_t<decltype(val)>::value_type;
  using value_type = std::decay_t<decltype(T::second)>;

  std::array<std::pair<const char*, value_type>, N> choices_cstr;
  for(int i = 0; i < N; i++)
  {
    choices_cstr[i].first = val[i].first.data();
    choices_cstr[i].second = val[i].second;
  }
  return choices_cstr;
}
template<std::size_t N, typename T>
consteval auto to_const_char_array(const std::array<std::pair<std::string_view, T>, N>& val)
{
  std::array<const char*, N> choices_cstr;
  for(int i = 0; i < N; i++)
    choices_cstr[i] = val[i].data();
  return choices_cstr;
}
template<std::size_t N>
consteval auto to_const_char_array(const std::string_view(&val)[N])
{
  std::array<const char*, N> choices_cstr;
  for(int i = 0; i < N; i++)
    choices_cstr[i] = val[i].data();
  return choices_cstr;
}
template<std::size_t N>
consteval auto to_const_char_array(const std::array<std::string_view, N>& val)
{
  std::array<const char*, N> choices_cstr;
  for(int i = 0; i < N; i++)
    choices_cstr[i] = val[i].data();
  return choices_cstr;
}

template<typename T>
consteval auto make_control_in()
{
  using value_type = as_type(T::value);
  constexpr auto name = avnd::get_name<T>().data();
  constexpr auto widg = avnd::get_widget<T>();

  // FIXME log normalization & friends

  if constexpr(widg.widget == avnd::widget_type::bang)
  {
    return Control::ImpulseButton{name};
  }
  else if constexpr(widg.widget == avnd::widget_type::button)
  {
    constexpr auto c = avnd::get_range<T>();
    return Control::Button{name};
  }
  else if constexpr(widg.widget == avnd::widget_type::toggle)
  {
    constexpr auto c = avnd::get_range<T>();
    if constexpr(requires { c.values(); }) {
       std::array<const char*, 2> arr{ c.values[0], c.values[1] };
       return Control::ChooserToggle{name, arr, c.init};
    }
    else {
      return Control::Toggle{name, c.init};
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::slider)
  {
    constexpr auto c = avnd::get_range<T>();
    if constexpr(std::is_integral_v<value_type>)
    {
      return Control::IntSlider{name, c.min, c.max, c.init};
    }
    else
    {
      return Control::FloatSlider{name, c.min, c.max, c.init};
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::spinbox)
  {
    constexpr auto c = avnd::get_range<T>();
    if constexpr(std::is_integral_v<value_type>)
    {
      return Control::IntSpinBox{name, c.min, c.max, c.init};
    }
    else
    {
      // FIXME do a FloatSpinBox
      return Control::FloatSlider{name, c.min, c.max, c.init};
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::knob)
  {
    constexpr auto c = avnd::get_range<T>();
    if constexpr(std::is_integral_v<value_type>)
    {
      // FIXME do a IntKnob
      return Control::IntSlider{name, c.min, c.max, c.init};
    }
    else
    {
      return Control::FloatKnob{name, c.min, c.max, c.init};
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::lineedit)
  {
    constexpr auto c = avnd::get_range<T>();
    return Control::LineEdit{name, c.init.data()};
  }
  else if constexpr(widg.widget == avnd::widget_type::combobox)
  {
    constexpr auto c = avnd::get_range<T>();
    if constexpr(std::is_enum_v<value_type>) {
      // List of string, values will be the strings themselves or 0, 1...
      // Convert from std::string_view to const char* until we migrate everything here
      return Control::ComboBox{name, c.init, to_const_char_array(T::choices())};
    }
    else if constexpr(requires { std::string_view{c.values[0].first}; }) {
      // Pair of string <-> values
      return Control::ComboBox{name, c.init, to_const_char_array(c.values)};
    }
    else if constexpr(requires { std::string_view{c.values[0]}; }) {
      // List of string, values will be the strings themselves or 0, 1...
      return Control::ComboBox{name, c.init, to_const_char_array(c.values)};
    }
    else if constexpr(requires { int{c.values[0]}; }) {
      // List of string, values will be the strings themselves or 0, 1...
      return Control::ComboBox{name, c.init, to_const_char_array(c.values)};
    }
    else {
      throw;
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::choices)
  {
    constexpr auto c = avnd::get_range<T>();
    if constexpr(std::is_enum_v<value_type>) {
      // List of string, values will be the strings themselves or 0, 1...
      // Convert from std::string_view to const char* until we migrate everything here
      return Control::Enum{name, c.init, to_const_char_array(T::choices())};
    }
    else if constexpr(requires { std::string_view{c.values[0].first}; }) {
      // Pair of string <-> values
      return Control::Enum{name, c.init, to_const_char_array(c.values)};
    }
    else if constexpr(requires { std::string_view{c.values[0]}; }) {
      // List of string, values will be the strings themselves or 0, 1...
      return Control::Enum{name, c.init, to_const_char_array(c.values)};
    }
    else if constexpr(requires { int{c.values[0]}; }) {
      // List of string, values will be the strings themselves or 0, 1...
      return Control::Enum{name, c.init, to_const_char_array(c.values)};
    }
    else {
      throw;
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::xy)
  {
    constexpr auto c = avnd::get_range<T>();
    return Control::XYSlider{name, c.min, c.max, c.init};
  }
  else if constexpr(widg.widget == avnd::widget_type::color)
  {
    constexpr auto c = avnd::get_range<T>();
    constexpr auto i = c.init;
    return Control::HSVSlider{name, {i.r, i.g, i.b, i.a}};
  }
  else
  {
    throw;
  }
}

template<typename T>
constexpr auto make_control_in(const T& t)
{ return make_control_in<T>(); }


template<typename T>
consteval auto make_control_out()
{
  using value_type = as_type(T::value);
  constexpr auto name = avnd::get_name<T>().data();
  constexpr auto widg = avnd::get_widget<T>();

  // FIXME log normalization & friends

  if constexpr(widg.widget == avnd::widget_type::bargraph)
  {
    constexpr auto c = avnd::get_range<T>();
    return Control::Bargraph{name, c.min, c.max, c.init};
  }
  else if constexpr(avnd::fp_ish<decltype(T::value)>)
  {
    constexpr auto c = avnd::get_range<T>();
    return Control::Bargraph{name, c.min, c.max, c.init};
  }
  else
  {
    static_assert(T::error__unsupported_widget_type);
  }
}

template<typename T>
constexpr auto make_control_out(const T& t)
{ return make_control_out<T>(); }
}

namespace oscr
{
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
  enum format { RGBA };
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

static_assert(avnd::cpu_texture<rgba_texture>);
}

