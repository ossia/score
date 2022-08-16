#pragma once
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/ProcessFlags.hpp>
#include <Process/ProcessMetadata.hpp>

#include <score/plugins/UuidKey.hpp>

#include <ossia/dataflow/audio_port.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/safe_nodes/tick_policies.hpp>
#include <ossia/detail/span.hpp>

#include <boost/container/vector.hpp>

#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/struct_reflection.hpp>
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/channels.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/concepts/midi_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>

#include <cmath>

#include <type_traits>

#define make_uuid(text) score::uuids::string_generator::compute((text))
#if defined(_MSC_VER)
#define uuid_constexpr inline
#else
#define uuid_constexpr constexpr
#endif

namespace oscr
{
template <typename Node, typename FieldIndex>
struct CustomFloatControl;
}

template <typename Node, typename FieldIndex>
struct is_custom_serialized<oscr::CustomFloatControl<Node, FieldIndex>> : std::true_type
{
};

namespace oscr
{
template <typename N>
consteval score::uuid_t uuid_from_string()
{
  if constexpr(requires {
                 {
                   N::uuid()
                   } -> std::convertible_to<score::uuid_t>;
               })
  {
    return N::uuid();
  }
  else
  {
    constexpr const char* str = N::uuid();
    return score::uuids::string_generator::compute(str, str + 37);
  }
}

template <typename Node>
score::uuids::uuid make_field_uuid(uint64_t is_input, uint64_t index)
{
  score::uuid_t node_uuid = uuid_from_string<Node>();
  uint64_t dat[2];

  memcpy(dat, node_uuid.data, 16);

  dat[0] ^= is_input;
  dat[1] ^= index;

  memcpy(node_uuid.data, dat, 16);

  return node_uuid;
}

template <typename Node, typename FieldIndex>
struct CustomFloatControl : public Process::ControlInlet
{
  static key_type static_concreteKey() noexcept
  {
    return make_field_uuid<Node>(true, FieldIndex{});
  }
  key_type concreteKey() const noexcept override { return static_concreteKey(); }
  void serialize_impl(const VisitorVariant& vis) const noexcept override
  {
    score::serialize_dyn(vis, *this);
  }

  CustomFloatControl(
      float min, float max, float init, const QString& name, Id<Process::Port> id,
      QObject* parent)
      : ControlInlet{id, parent}
  {
    hidden = true;
    setValue(init);
    setDomain(ossia::make_domain(min, max));
    setName(name);
  }

  ~CustomFloatControl() { }

  auto getMin() const noexcept { return domain().get().template convert_min<float>(); }
  auto getMax() const noexcept { return domain().get().template convert_max<float>(); }
  void setupExecution(ossia::inlet& inl) const noexcept override
  {
    auto& port = **safe_cast<ossia::value_inlet*>(&inl);
    port.type = ossia::val_type::FLOAT;
    port.domain = domain().get();
  }

  using Process::ControlInlet::ControlInlet;
};

}
template <typename Node, typename FieldIndex>
struct TSerializer<DataStream, oscr::CustomFloatControl<Node, FieldIndex>>
{
  using model_type = oscr::CustomFloatControl<Node, FieldIndex>;
  static void readFrom(DataStream::Serializer& s, const model_type& p)
  {
    s.read((const Process::ControlInlet&)p);
  }

  static void writeTo(DataStream::Deserializer& s, model_type& eff) { }
};

template <typename Node, typename FieldIndex>
struct TSerializer<JSONObject, oscr::CustomFloatControl<Node, FieldIndex>>
{
  using model_type = oscr::CustomFloatControl<Node, FieldIndex>;
  static void readFrom(JSONObject::Serializer& s, const model_type& p)
  {
    s.read((const Process::ControlInlet&)p);
  }

  static void writeTo(JSONObject::Deserializer& s, model_type& eff) { }
};

namespace oscr
{

template <typename T, std::size_t N>
constexpr auto to_const_char_array(const T (&val)[N])
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
template <std::size_t N, typename T>
constexpr auto
to_const_char_array(const std::array<std::pair<std::string_view, T>, N>& val)
{
  std::array<const char*, N> choices_cstr;
  for(int i = 0; i < N; i++)
    choices_cstr[i] = val[i].data();
  return choices_cstr;
}
template <std::size_t N>
constexpr auto to_const_char_array(const std::string_view (&val)[N])
{
  std::array<const char*, N> choices_cstr;
  for(int i = 0; i < N; i++)
    choices_cstr[i] = val[i].data();
  return choices_cstr;
}
template <std::size_t N>
constexpr auto to_const_char_array(const std::array<std::string_view, N>& val)
{
  std::array<const char*, N> choices_cstr;
  for(int i = 0; i < N; i++)
    choices_cstr[i] = val[i].data();
  return choices_cstr;
}

std::vector<std::pair<QString, ossia::value>> to_combobox_range(const auto& in)
{
  std::vector<std::pair<QString, ossia::value>> vec;
  for(auto& v : to_const_char_array(in))
    vec.emplace_back(v.first, v.second);
  return vec;
}

std::vector<std::string> to_enum_range(const auto& in)
{
  std::vector<std::string> vec;
  for(auto& v : to_const_char_array(in))
    vec.emplace_back(v);
  return vec;
}

template <typename Node, typename T, std::size_t N>
auto make_control_in(avnd::field_index<N>, Id<Process::Port>&& id, QObject* parent)
{
  using value_type = as_type(T::value);
  constexpr auto name = avnd::get_name<T>();
  QString qname = QString::fromUtf8(name.data(), name.size());

  constexpr auto widg = avnd::get_widget<T>();

  // FIXME log normalization & friends

  if constexpr(widg.widget == avnd::widget_type::bang)
  {
    return new Process::ImpulseButton{qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::button)
  {
    return new Process::Button{qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::toggle)
  {
    constexpr auto c = avnd::get_range<T>();
    if constexpr(requires { c.values(); })
    {
      return new Process::ChooserToggle{
          {c.values[0], c.values[1]}, c.init, qname, id, parent};
    }
    else
    {
      return new Process::Toggle{bool(c.init), qname, id, parent};
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::slider)
  {
    constexpr auto c = avnd::get_range<T>();
    if constexpr(std::is_integral_v<value_type>)
    {
      return new Process::IntSlider{c.min, c.max, c.init, qname, id, parent};
    }
    else
    {
      if constexpr(avnd::has_mapper<T>)
      {
        return new CustomFloatControl<Node, avnd::field_index<N>>{c.min, c.max, c.init,
                                                                  qname, id,    parent};
      }
      else
      {
        return new Process::FloatSlider{c.min, c.max, c.init, qname, id, parent};
      }
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::time_chooser)
  {
    constexpr auto c = avnd::get_range<T>();
    return new Process::TimeChooser{c.min, c.max, c.init, qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::range)
  {
    constexpr auto c = avnd::get_range<T>();
    return nullptr; // TODO
  }
  else if constexpr(widg.widget == avnd::widget_type::spinbox)
  {
    constexpr auto c = avnd::get_range<T>();
    if constexpr(std::is_integral_v<value_type>)
    {
      return new Process::IntSpinBox{c.min, c.max, c.init, qname, id, parent};
    }
    else
    {
      // FIXME do a FloatSpinBox
      return new Process::FloatSlider{c.min, c.max, c.init, qname, id, parent};
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::knob)
  {
    constexpr auto c = avnd::get_range<T>();
    if constexpr(std::is_integral_v<value_type>)
    {
      // FIXME do a IntKnob
      return new Process::IntSlider{c.min, c.max, c.init, qname, id, parent};
    }
    else
    {
      if constexpr(avnd::has_mapper<T>)
      {
        return new CustomFloatControl<Node, avnd::field_index<N>>{c.min, c.max, c.init,
                                                                  qname, id,    parent};
      }
      else
      {
        return new Process::FloatKnob{c.min, c.max, c.init, qname, id, parent};
      }
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::lineedit)
  {
    constexpr auto c = avnd::get_range<T>();
    return new Process::LineEdit{c.init.data(), qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::combobox)
  {
    constexpr auto c = avnd::get_range<T>();
    return new Process::ComboBox{to_combobox_range(c.values), c.init, qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::choices)
  {
    constexpr auto c = avnd::get_range<T>();
    auto enums = to_enum_range(c.values);
    auto init = enums[c.init];
    std::vector<QString> pixmaps;
    if constexpr(requires { c.pixmaps; })
    {
      for(auto& pix : c.pixmaps)
      {
        pixmaps.push_back(QString::fromLatin1(pix.data(), pix.size()));
      }
    }
    return new Process::Enum{
        std::move(enums), pixmaps, std::move(init), qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::xy)
  {
    constexpr auto c = avnd::get_range<T>();
    if constexpr(requires {
                   c.min == 0.f;
                   c.max == 0.f;
                   c.init == 0.f;
                 })
    {
      return new Process::XYSlider{
          {c.min, c.min}, {c.max, c.max}, {c.init, c.init}, qname, id, parent};
    }
    else
    {
      auto [mx, my] = c.min;
      auto [Mx, My] = c.max;
      auto [ix, iy] = c.init;
      return new Process::XYSlider{{mx, my}, {Mx, My}, {ix, iy}, qname, id, parent};
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::color)
  {
    constexpr auto c = avnd::get_range<T>();
    constexpr auto i = c.init;
    return new Process::HSVSlider{{i.r, i.g, i.b, i.a}, qname, id, parent};
  }
  else
  {
    static_assert(T::is_not_a_valid_control);
  }
}

template <typename T, std::size_t N>
auto make_control_out(avnd::field_index<N>, Id<Process::Port>&& id, QObject* parent)
{
  using value_type = as_type(T::value);
  constexpr auto name = avnd::get_name<T>();
  constexpr auto widg = avnd::get_widget<T>();
  QString qname = QString::fromUtf8(name.data(), name.size());

  // FIXME log normalization & friends

  if constexpr(widg.widget == avnd::widget_type::bargraph)
  {
    constexpr auto c = avnd::get_range<T>();
    return new Process::Bargraph{c.min, c.max, c.init, qname, id, parent};
  }
  else if constexpr(avnd::fp_ish<decltype(T::value)>)
  {
    constexpr auto c = avnd::get_range<T>();
    return new Process::Bargraph{c.min, c.max, c.init, qname, id, parent};
  }
  else
  {
    static_assert(T::is_not_a_valid_control);
  }
}

template <typename T>
constexpr auto make_control_out(const T& t)
{
  return make_control_out<T>();
}
}

namespace oscr
{
struct multichannel_audio_view
{
  ossia::audio_vector* buffer{};
  int64_t offset{};
  int64_t duration{};

  tcb::span<const double> operator[](std::size_t i) const noexcept
  {
    auto& chan = (*buffer)[i];
    int64_t min_dur = std::min(int64_t(chan.size()) - offset, duration);
    if(min_dur < 0)
      min_dur = 0;

    return tcb::span<const double>{chan.data() + offset, std::size_t(min_dur)};
  }

  std::size_t channels() const noexcept { return buffer->size(); }
  void resize(std::size_t i) const noexcept { return buffer->resize(i); }
  void reserve(std::size_t channels, std::size_t bufferSize)
  {
    resize(channels);
    for(auto& vec : *buffer)
      vec.reserve(bufferSize);
  }
};

struct multichannel_audio
{
  ossia::audio_vector* buffer{};
  int64_t offset{};
  int64_t duration{};

  tcb::span<double> operator[](std::size_t i) const noexcept
  {
    auto& chan = (*buffer)[i];
    int64_t min_dur = std::min(int64_t(chan.size()) - offset, duration);
    if(min_dur < 0)
      min_dur = 0;

    return tcb::span<double>{chan.data() + offset, std::size_t(min_dur)};
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

struct rgba_texture
{
  enum format
  {
    RGBA
  };
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
  {
    bytes = data;
    width = w;
    height = h;
    changed = true;
  }
};

static_assert(avnd::cpu_texture<rgba_texture>);
}
