#pragma once
#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/plugins/DeserializeKnownSubType.hpp>

//#include <ossia/dataflow/port.hpp>
#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/dataflow/safe_nodes/port.hpp>
#include <ossia/detail/timed_vec.hpp>

#include <score_lib_process_export.h>
namespace Control
{

struct OutControl final : ossia::safe_nodes::control_out
{
  static const constexpr bool must_validate = false;
  using type = ossia::value;

  constexpr OutControl(const char* name)
      : ossia::safe_nodes::control_out{name}
  {
  }

  auto create_outlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::ControlOutlet{id, parent};
  }
  auto create_outlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::ControlOutlet>(id, parent);
  }
  auto create_outlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::ControlOutlet>(id, parent);
  }

  const ossia::value& fromValue(const ossia::value& v) const { return v; }
  const ossia::value& toValue(const ossia::value& v) const { return v; }
};

struct InControl final : ossia::safe_nodes::control_in
{
  static const constexpr bool must_validate = false;
  using type = ossia::value;

  constexpr InControl(const char* name)
    : ossia::safe_nodes::control_in{name}
  {
  }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::ControlInlet{id, parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::ControlInlet>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::ControlInlet>(id, parent);
  }

  void setup_exec(auto& v) const
  {
  }
  const ossia::value& fromValue(const ossia::value& v) const { return v; }
  const ossia::value& toValue(const ossia::value& v) const { return v; }
};

template <typename Model_T, typename T>
struct FloatControl final
    : ossia::safe_nodes::control_in
    , WidgetFactory::FloatControl<T, WidgetFactory::LinearNormalizer, true>
{
  static const constexpr bool must_validate = false;
  using type = float;
  const float min{};
  const float max{};
  const float init{};

  constexpr FloatControl(const char* name, float v1, float v2, float v3)
      : ossia::safe_nodes::control_in{name}
      , min{v1}
      , max{v2}
      , init{v3}
  {
  }

  auto getMin() const { return min; }
  auto getMax() const { return max; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Model_T{
        min,
        max,
        init,
        QString::fromUtf8(name.data(), name.size()),
        id,
        parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Model_T>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Model_T>(id, parent);
  }

  void setup_exec(auto& v) const
  {
    v->type = ossia::val_type::FLOAT;
    v->domain = ossia::domain_base<float>(this->min, this->max);
  }

  float fromValue(const ossia::value& v) const
  {
    return ossia::convert<float>(v);
  }
  ossia::value toValue(float v) const { return v; }
};

template <typename Model_T, typename T>
struct LogFloatControl final
    : ossia::safe_nodes::control_in
    , WidgetFactory::FloatControl<T, WidgetFactory::LogNormalizer, true>
{
  static const constexpr bool must_validate = false;
  using type = float;
  const float min{};
  const float max{};
  const float init{};

  constexpr LogFloatControl(
      const char* name,
      float v1,
      float v2,
      float v3)
      : ossia::safe_nodes::control_in{name}
      , min{v1}
      , max{v2}
      , init{v3}
  {
  }

  auto getMin() const { return min; }
  auto getMax() const { return max; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Model_T{
        min,
        max,
        init,
        QString::fromUtf8(name.data(), name.size()),
        id,
        parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Model_T>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Model_T>(id, parent);
  }

  void setup_exec(auto& v) const
  {
    v->type = ossia::val_type::FLOAT;
    v->domain = ossia::domain_base<float>(this->min, this->max);
  }

  float fromValue(const ossia::value& v) const
  {
    return ossia::convert<float>(v);
  }
  ossia::value toValue(float v) const { return v; }
};

template <typename T>
struct FloatDisplay final
    : ossia::safe_nodes::control_out
    , WidgetFactory::FloatControl<T, WidgetFactory::LinearNormalizer, false>
{
  static const constexpr bool must_validate = false;
  using type = float;
  const float min{};
  const float max{};
  const float init{};

  constexpr FloatDisplay(const char *name, float v1, float v2, float v3)
      : ossia::safe_nodes::control_out{name}
      , min{v1}
      , max{v2}
      , init{v3}
  {
  }

  auto getMin() const { return min; }
  auto getMax() const { return max; }

  auto create_outlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::Bargraph{
        min,
        max,
        init,
        QString::fromUtf8(name.data(), name.size()),
        id,
        parent};
  }
  auto create_outlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::Bargraph>(id, parent);
  }
  auto create_outlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::Bargraph>(id, parent);
  }

  void setup_exec(auto& v) const
  {
    v->type = ossia::val_type::FLOAT;
    v->domain = ossia::domain_base<float>(this->min, this->max);
  }

  float fromValue(const ossia::value& v) const
  {
    return ossia::convert<float>(v);
  }
  ossia::value toValue(float v) const { return v; }
};

using FloatSlider = FloatControl<Process::FloatSlider, score::QGraphicsSlider>;
using FloatKnob = FloatControl<Process::FloatKnob, score::QGraphicsKnob>;
using LogFloatSlider = LogFloatControl<Process::LogFloatSlider, score::QGraphicsLogSlider>;
using LogFloatKnob = LogFloatControl<Process::LogFloatSlider, score::QGraphicsLogKnob>;
// FIXME the process implementation is missing.

using Bargraph = FloatDisplay<score::QGraphicsSlider>;

struct IntSlider final
    : ossia::safe_nodes::control_in
    , WidgetFactory::IntSlider
{
  using type = int;
  const int min{};
  const int max{};
  const int init{};

  static const constexpr bool must_validate = false;
  constexpr IntSlider(const char* name, int v1, int v2, int v3)
      : ossia::safe_nodes::control_in{name}
      , min{v1}
      , max{v2}
      , init{v3}
  {
  }

  int fromValue(const ossia::value& v) const { return ossia::convert<int>(v); }
  ossia::value toValue(int v) const { return v; }

  auto getMin() const { return min; }
  auto getMax() const { return max; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::IntSlider{
        min,
        max,
        init,
        QString::fromUtf8(name.data(), name.size()),
        id,
        parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::IntSlider>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::IntSlider>(id, parent);
  }

  void setup_exec(auto& v) const
  {
    v->type = ossia::val_type::INT;
    v->domain = ossia::domain_base<int>(this->min, this->max);
  }
};

struct IntSpinBox final
    : ossia::safe_nodes::control_in
    , WidgetFactory::IntSpinBox
{
  static const constexpr bool must_validate = false;
  using type = int;
  const int min{};
  const int max{};
  const int init{};

  int fromValue(const ossia::value& v) const { return ossia::convert<int>(v); }
  ossia::value toValue(int v) const { return v; }

  constexpr IntSpinBox(const char* name, int v1, int v2, int v3)
      : ossia::safe_nodes::control_in{name}
      , min{v1}
      , max{v2}
      , init{v3}
  {
  }

  auto getMin() const { return min; }
  auto getMax() const { return max; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::IntSpinBox{
        min,
        max,
        init,
        QString::fromUtf8(name.data(), name.size()),
        id,
        parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::IntSpinBox>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::IntSpinBox>(id, parent);
  }
  void setup_exec(auto& v) const
  {
    v->type = ossia::val_type::INT;
    v->domain = ossia::domain_base<int>(this->min, this->max);
  }
};

struct Button final
    : ossia::safe_nodes::control_in
    , WidgetFactory::Button
{
  static const constexpr bool must_validate = false;

  constexpr Button(const char *name)
      : ossia::safe_nodes::control_in{name}
  {
  }

  using type = bool;
  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::Button{
        QString::fromUtf8(name.data(), name.size()), id, parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::Button>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::Button>(id, parent);
  }

  bool fromValue(const ossia::value& v) const
  {
    return ossia::convert<bool>(v);
  }
  ossia::value toValue(bool v) const { return v; }

  void setup_exec(auto& v) const
  {
    v->type = ossia::val_type::BOOL;
    v->domain = ossia::domain_base<bool>();
  }
};

struct ImpulseButton final
    : ossia::safe_nodes::control_in
    , WidgetFactory::ImpulseButton
{
  static const constexpr bool must_validate = false;
  constexpr ImpulseButton(const char* name)
      : ossia::safe_nodes::control_in{name}
  {
  }

  using type = ossia::impulse;
  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::ImpulseButton{
        QString::fromUtf8(name.data(), name.size()), id, parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::ImpulseButton>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::ImpulseButton>(id, parent);
  }

  ossia::impulse fromValue(const ossia::value& v) const
  {
    return {};
  }
  ossia::value toValue(const ossia::value& v) const { return ossia::impulse{}; }

  void setup_exec(auto& v) const
  {
    v->type = ossia::val_type::BOOL;
    v->domain = ossia::domain_base<ossia::impulse>();
  }
};

struct Toggle final
    : ossia::safe_nodes::control_in
    , WidgetFactory::Toggle
{
  static const constexpr bool must_validate = false;
  constexpr Toggle(const char* name, bool v1)
      : ossia::safe_nodes::control_in{name}
      , init{v1}
  {
  }

  using type = bool;
  const bool init{};
  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::Toggle{
        init, QString::fromUtf8(name.data(), name.size()), id, parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::Toggle>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::Toggle>(id, parent);
  }

  bool fromValue(const ossia::value& v) const
  {
    return ossia::convert<bool>(v);
  }
  ossia::value toValue(bool v) const { return v; }

  void setup_exec(auto& v) const
  {
    v->type = ossia::val_type::BOOL;
    v->domain = ossia::domain_base<bool>();
  }
};

struct ChooserToggle final
    : ossia::safe_nodes::control_in
    , WidgetFactory::ChooserToggle
{
  static const constexpr bool must_validate = false;

  constexpr ChooserToggle(
      const char* name,
      std::array<const char*, 2> alt,
      bool v1)
      : ossia::safe_nodes::control_in{name}
      , alternatives{alt}
      , init{v1}
  {
  }
  using type = bool;
  std::array<const char*, 2> alternatives;
  const bool init{};

  bool fromValue(const ossia::value& v) const
  {
    return ossia::convert<bool>(v);
  }
  ossia::value toValue(bool v) const { return v; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::ChooserToggle{
        {alternatives[0], alternatives[1]},
        init,
        QString::fromUtf8(name.data(), name.size()),
        id,
        parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::ChooserToggle>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::ChooserToggle>(id, parent);
  }
  void setup_exec(auto& v) const
  {
    v->type = ossia::val_type::BOOL;
    v->domain = ossia::domain_base<bool>();
  }
};
struct LineEdit final
    : ossia::safe_nodes::control_in
    , WidgetFactory::LineEdit
{
  static const constexpr bool must_validate = false;
  constexpr LineEdit(const char* name, const char* init)
      : ossia::safe_nodes::control_in{name}
      , init{init}
  {
  }

  std::string fromValue(const ossia::value& v) const
  {
    return ossia::convert<std::string>(v);
  }
  ossia::value toValue(std::string v) const
  {
    return ossia::value{std::move(v)};
  }

  using type = std::string;
  const std::string_view init{};
  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::LineEdit{
        init.data(), QString::fromUtf8(name.data(), name.size()), id, parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::LineEdit>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::LineEdit>(id, parent);
  }
  void setup_exec(auto& v) const
  {
    v->type = ossia::val_type::STRING;
  }
};

template <typename T, std::size_t N>
struct ComboBox final
    : ossia::safe_nodes::control_in
    , WidgetFactory::ComboBox
{
  static const constexpr bool must_validate = false;
  using type = T;
  const std::size_t init{};
  const std::array<std::pair<const char*, T>, N> values;

  static constexpr auto count() { return N; }

  constexpr ComboBox(const char* name, std::size_t in, std::array<std::pair<const char*, T>, N> arr)
      : ossia::safe_nodes::control_in{name}
      , init{in}
      , values{arr}
  {
  }

  const auto& getValues() const { return values; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    std::vector<std::pair<QString, ossia::value>> vec;
    for (auto& v : values)
      vec.emplace_back(v.first, v.second);

    return new Process::ComboBox(
        vec,
        values[init].second,
        QString::fromUtf8(name.data(), name.size()),
        id,
        parent);
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::ComboBox>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::ComboBox>(id, parent);
  }

  T fromValue(const ossia::value& v) const { return ossia::convert<T>(v); }
  ossia::value toValue(T v) const { return ossia::value{std::move(v)}; }

  void setup_exec(auto& v) const
  {
    v->type = ossia::val_type::STRING;
  }
};

template <std::size_t N>
struct EnumBase
    : ossia::safe_nodes::control_in
    , WidgetFactory::Enum
{
  using type = std::string;
  const std::size_t init{};
  const std::array<const char*, N> values;
  using Pixmaps_T = std::array<const char*, 2 * N>;
  Pixmaps_T pixmaps;

  const auto& getValues() const { return values; }

  constexpr EnumBase(const char* name, std::size_t i, const std::array<const char*, N>& v)
      : ossia::safe_nodes::control_in{name}
      , init{i}
      , values{v}
      , pixmaps{}
  {
  }

  constexpr EnumBase(
      const char* name,
      std::size_t i,
      const std::array<const char*, N>& v,
      const Pixmaps_T& pixmaps)
      : ossia::safe_nodes::control_in{name}
      , init{i}
      , values{v}
      , pixmaps{pixmaps}
  {
  }

  ossia::value toValue(std::string v) const
  {
    return ossia::value{std::move(v)};
  }

  ossia::value toValue(int v) const
  {
    if(ossia::valid_index(v, values))
      return ossia::value{std::string(values[v])};
    else
      return ossia::value{};
  }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::Enum{
        std::vector<std::string>(values.begin(), values.end()),
        pixmaps[0] == nullptr
            ? std::vector<QString>{}
            : std::vector<QString>{pixmaps.begin(), pixmaps.end()},
        values[init],
        QString::fromUtf8(name.data(), name.size()),
        id,
        parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::Enum>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::Enum>(id, parent);
  }

  void setup_exec(auto& v) const
  {
  }
};

template <std::size_t N>
struct Enum final : EnumBase<N>
{
  constexpr Enum(const char* name, std::size_t i, const std::array<const char*, N>& v)
      : EnumBase<N>{name, i, v}
  {
  }

  constexpr Enum(
      const char* name,
      std::size_t i,
      const std::array<const char*, N>& v,
      const typename EnumBase<N>::Pixmaps_T& pixmaps)
      : EnumBase<N>{name, i, v, pixmaps}
  {
  }

  static const constexpr bool must_validate = true;

  void convert(const ossia::timed_vec<std::string>& source, ossia::timed_vec<std::string>& sink) {
    sink = source;
  }

  template<typename Sink>
  void convert(const ossia::timed_vec<std::string>& source, ossia::timed_vec<Sink>& sink) {
    sink.clear();
    sink.container.reserve(source.size());
    for(auto& [timestamp, value] : source) {
      if(auto it = ossia::find(this->values, value); it != this->values.end()) {
        sink[timestamp] = static_cast<Sink>(it - this->values.begin());
      }
    }
  }
  bool fromValue(const ossia::value& v, std::string& str) const
  {
    switch(v.get_type())
    {
      case ossia::val_type::INT:
      {
        int t = *v.target<int>();
        if(ossia::valid_index(t, this->values))
        {
          str = this->values[t];
          return true;
        }
        break;
      }
      case ossia::val_type::STRING:
      {
        auto t = v.target<std::string>();
        if (auto it = ossia::find(this->values, *t); it != this->values.end())
        {
          str = *t;
          return true;
        }
        break;
      }
      default:
        break;
    }

    return false;
  }
  template<typename T>
  bool fromValue(const ossia::value& v, T& integer_like) const
  {
    switch(v.get_type())
    {
      case ossia::val_type::INT:
      {
        int t = *v.target<int>();
        if(ossia::valid_index(t, this->values))
        {
          integer_like = t;
          return true;
        }
        break;
      }
      case ossia::val_type::STRING:
      {
        auto t = v.target<std::string>();
        if (auto it = ossia::find(this->values, *t); it != this->values.end())
        {
          integer_like = it - this->values.begin();
          return true;
        }
        break;
      }
      default:
        break;
    }

    return false;
  }

  auto fromValue(const ossia::value& v) const
  {
    auto t = v.target<std::string>();
    if (t)
    {
      if (auto it = ossia::find(this->values, *t); it != this->values.end())
      {
        return std::optional<std::string>{*t};
      }
    }
    return std::optional<std::string>{};
  }
};

template <std::size_t N>
struct UnvalidatedEnum final : EnumBase<N>
{
  constexpr UnvalidatedEnum(const char* name, std::size_t i, const std::array<const char*, N>& v)
      : EnumBase<N>{name, i, v}
  {
  }

  constexpr UnvalidatedEnum(
      const char* name,
      std::size_t i,
      const std::array<const char*, N>& v,
      const typename EnumBase<N>::Pixmaps_T& pixmaps)
      : EnumBase<N>{name, i, v, pixmaps}
  {
  }

  static const constexpr bool must_validate = false;
  auto fromValue(const ossia::value& v) const
  {
    auto t = v.target<std::string>();
    if (t)
      return *t;
    return std::string{};
  }
};

template <typename T1, typename T2>
constexpr auto make_enum(const T1& t1, std::size_t s, const T2& t2)
{
  return Control::Enum<T2{}.size()>(t1, s, t2);
}
template <typename T1, typename T2, typename Pixmaps_T>
constexpr auto
make_enum(const T1& t1, std::size_t s, const T2& t2, const Pixmaps_T& pixmaps)
{
  return Control::Enum<T2{}.size()>(t1, s, t2, pixmaps);
}
template <typename T1, typename T2>
constexpr auto make_unvalidated_enum(const T1& t1, std::size_t s, const T2& t2)
{
  return Control::UnvalidatedEnum<T2{}.size()>(t1, s, t2);
}

struct XYSlider final
    : ossia::safe_nodes::control_in
    , WidgetFactory::XYSlider
{
  static const constexpr bool must_validate = false;
  constexpr XYSlider(const char* name, ossia::vec2f min, ossia::vec2f max, ossia::vec2f init = {})
      : ossia::safe_nodes::control_in{name}
      , min{min}
      , max{max}
      , init{init}
  {
  }
  constexpr XYSlider(const char* name, float min, float max, float init = {})
      : ossia::safe_nodes::control_in{name}
      , min{min, min}
      , max{max, max}
      , init{init, init}
  {
  }

  using type = ossia::vec2f;
  const ossia::vec2f min, max, init{};
  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::XYSlider{
        min, max, init, QString::fromUtf8(name.data(), name.size()), id, parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::XYSlider>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::XYSlider>(id, parent);
  }

  ossia::vec2f fromValue(const ossia::value& v) const
  {
    return ossia::convert<ossia::vec2f>(v);
  }
  ossia::value toValue(ossia::vec2f v) const { return v; }

  void setup_exec(auto& v) const
  {
    v->domain = ossia::vecf_domain<2>(min, max);
    v->type = ossia::val_type::VEC2F;
  }
};
struct HSVSlider final
    : ossia::safe_nodes::control_in
    , WidgetFactory::HSVSlider
{
  static const constexpr bool must_validate = false;
  constexpr HSVSlider(const char* name, ossia::vec4f init = {})
      : ossia::safe_nodes::control_in{name}
      , init{init}
  {
  }

  using type = ossia::vec4f;
  const ossia::vec4f  init{};
  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::HSVSlider{
        init, QString::fromUtf8(name.data(), name.size()), id, parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::HSVSlider>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::HSVSlider>(id, parent);
  }

  ossia::vec4f fromValue(const ossia::value& v) const
  {
    return ossia::convert<ossia::vec4f>(v);
  }
  ossia::value toValue(ossia::vec4f v) const { return v; }

  void setup_exec(auto& v) const
  {
    v->type = ossia::rgba_u{};
  }
};

}
