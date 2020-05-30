#pragma once
#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/plugins/DeserializeKnownSubType.hpp>

#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/safe_nodes/port.hpp>

#include <score_lib_process_export.h>
namespace Control
{

struct OutControl final : ossia::safe_nodes::control_out
{
  static const constexpr bool must_validate = false;
  using type = ossia::value;
  using port_type = Process::ControlOutlet;

  template <std::size_t N>
  constexpr OutControl(const char (&name)[N]) : ossia::safe_nodes::control_out{name}
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

template <typename T>
struct FloatControl final : ossia::safe_nodes::control_in, WidgetFactory::FloatControl<T>
{
  static const constexpr bool must_validate = false;
  using type = float;
  using port_type = Process::FloatSlider;
  const float min{};
  const float max{};
  const float init{};

  template <std::size_t N>
  constexpr FloatControl(const char (&name)[N], float v1, float v2, float v3)
      : ossia::safe_nodes::control_in{name}, min{v1}, max{v2}, init{v3}
  {
  }

  auto getMin() const { return min; }
  auto getMax() const { return max; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::FloatSlider{
        min, max, init, QString::fromUtf8(name.data(), name.size()), id, parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::FloatSlider>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::FloatSlider>(id, parent);
  }

  void setup_exec(ossia::value_inlet& v) const
  {
    v->domain = ossia::domain_base<float>(this->min, this->max);
  }

  float fromValue(const ossia::value& v) const { return ossia::convert<float>(v); }
  ossia::value toValue(float v) const { return v; }
};

template <typename T>
struct LogFloatControl final : ossia::safe_nodes::control_in, WidgetFactory::LogFloatControl<T>
{
  static const constexpr bool must_validate = false;
  using type = float;
  using port_type = Process::LogFloatSlider; // TODO BUG
  const float min{};
  const float max{};
  const float init{};

  template <std::size_t N>
  constexpr LogFloatControl(const char (&name)[N], float v1, float v2, float v3)
      : ossia::safe_nodes::control_in{name}, min{v1}, max{v2}, init{v3}
  {
  }

  auto getMin() const { return min; }
  auto getMax() const { return max; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::LogFloatSlider{
        min, max, init, QString::fromUtf8(name.data(), name.size()), id, parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::LogFloatSlider>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::LogFloatSlider>(id, parent);
  }

  void setup_exec(ossia::value_inlet& v) const
  {
    v->domain = ossia::domain_base<float>(this->min, this->max);
  }

  float fromValue(const ossia::value& v) const { return ossia::convert<float>(v); }
  ossia::value toValue(float v) const { return v; }
};

using FloatSlider = FloatControl<score::QGraphicsSlider>;
using FloatKnob = FloatControl<score::QGraphicsKnob>;
using LogFloatSlider = LogFloatControl<score::QGraphicsLogSlider>;
using LogFloatKnob = LogFloatControl<score::QGraphicsLogKnob>;

struct IntSlider final : ossia::safe_nodes::control_in, WidgetFactory::IntSlider
{
  using type = int;
  using port_type = Process::IntSlider;
  const int min{};
  const int max{};
  const int init{};

  static const constexpr bool must_validate = false;
  template <std::size_t N>
  constexpr IntSlider(const char (&name)[N], int v1, int v2, int v3)
      : ossia::safe_nodes::control_in{name}, min{v1}, max{v2}, init{v3}
  {
  }

  int fromValue(const ossia::value& v) const { return ossia::convert<int>(v); }
  ossia::value toValue(int v) const { return v; }

  auto getMin() const { return min; }
  auto getMax() const { return max; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::IntSlider{
        min, max, init, QString::fromUtf8(name.data(), name.size()), id, parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::IntSlider>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::IntSlider>(id, parent);
  }

  void setup_exec(ossia::value_inlet& v) const
  {
    v->domain = ossia::domain_base<int>(this->min, this->max);
  }
};

struct IntSpinBox final : ossia::safe_nodes::control_in, WidgetFactory::IntSpinBox
{
  static const constexpr bool must_validate = false;
  using type = int;
  using port_type = Process::IntSpinBox;
  const int min{};
  const int max{};
  const int init{};

  int fromValue(const ossia::value& v) const { return ossia::convert<int>(v); }
  ossia::value toValue(int v) const { return v; }

  template <std::size_t N>
  constexpr IntSpinBox(const char (&name)[N], int v1, int v2, int v3)
      : ossia::safe_nodes::control_in{name}, min{v1}, max{v2}, init{v3}
  {
  }

  auto getMin() const { return min; }
  auto getMax() const { return max; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::IntSpinBox{
        min, max, init, QString::fromUtf8(name.data(), name.size()), id, parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::IntSpinBox>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::IntSpinBox>(id, parent);
  }
  void setup_exec(ossia::value_inlet& v) const
  {
    v->domain = ossia::domain_base<int>(this->min, this->max);
  }
};

struct Button final : ossia::safe_nodes::control_in, WidgetFactory::Button
{
  static const constexpr bool must_validate = false;
  template <std::size_t N>
  constexpr Button(const char (&name)[N]) : ossia::safe_nodes::control_in{name}
  {
  }

  using type = bool;
  using port_type = Process::Toggle;
  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::Button{QString::fromUtf8(name.data(), name.size()), id, parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::Button>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::Button>(id, parent);
  }

  bool fromValue(const ossia::value& v) const { return ossia::convert<bool>(v); }
  ossia::value toValue(bool v) const { return v; }

  void setup_exec(ossia::value_inlet& v) const { v->domain = ossia::domain_base<bool>(); }
};

struct Toggle final : ossia::safe_nodes::control_in, WidgetFactory::Toggle
{
  static const constexpr bool must_validate = false;
  template <std::size_t N>
  constexpr Toggle(const char (&name)[N], bool v1) : ossia::safe_nodes::control_in{name}, init{v1}
  {
  }

  using type = bool;
  using port_type = Process::Toggle;
  const bool init{};
  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::Toggle{init, QString::fromUtf8(name.data(), name.size()), id, parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::Toggle>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::Toggle>(id, parent);
  }

  bool fromValue(const ossia::value& v) const { return ossia::convert<bool>(v); }
  ossia::value toValue(bool v) const { return v; }

  void setup_exec(ossia::value_inlet& v) const { v->domain = ossia::domain_base<bool>(); }
};

struct ChooserToggle final : ossia::safe_nodes::control_in, WidgetFactory::ChooserToggle
{
  static const constexpr bool must_validate = false;
  template <std::size_t N>
  constexpr ChooserToggle(const char (&name)[N], std::array<const char*, 2> alt, bool v1)
      : ossia::safe_nodes::control_in{name}, alternatives{alt}, init{v1}
  {
  }
  using type = bool;
  using port_type = Process::ChooserToggle;
  std::array<const char*, 2> alternatives;
  const bool init{};

  bool fromValue(const ossia::value& v) const { return ossia::convert<bool>(v); }
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
  void setup_exec(ossia::value_inlet& v) const { v->domain = ossia::domain_base<bool>(); }
};
struct LineEdit final : ossia::safe_nodes::control_in, WidgetFactory::LineEdit
{
  static const constexpr bool must_validate = false;
  template <std::size_t N, std::size_t M>
  constexpr LineEdit(const char (&name)[N], const char (&init)[M])
      : ossia::safe_nodes::control_in{name}, init{init, M - 1}
  {
  }

  std::string fromValue(const ossia::value& v) const { return ossia::convert<std::string>(v); }
  ossia::value toValue(std::string v) const { return ossia::value{std::move(v)}; }

  using type = std::string;
  using port_type = Process::LineEdit;
  const QLatin1String init{};
  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::LineEdit{init, QString::fromUtf8(name.data(), name.size()), id, parent};
  }
  auto create_inlet(DataStream::Deserializer& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::LineEdit>(id, parent);
  }
  auto create_inlet(JSONObject::Deserializer&& id, QObject* parent) const
  {
    return deserialize_known_interface<Process::LineEdit>(id, parent);
  }
  void setup_exec(ossia::value_inlet& v) const { }
};

template <typename T, std::size_t N>
struct ComboBox final : ossia::safe_nodes::control_in, WidgetFactory::ComboBox
{
  static const constexpr bool must_validate = false;
  using type = T;
  using port_type = Process::ComboBox;
  const std::size_t init{};
  const std::array<std::pair<const char*, T>, N> values;

  static constexpr auto count() { return N; }
  template <std::size_t M, typename Arr>
  constexpr ComboBox(const char (&name)[M], std::size_t in, Arr arr)
      : ossia::safe_nodes::control_in{name}, init{in}, values{arr}
  {
  }

  const auto& getValues() const { return values; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    std::vector<std::pair<QString, ossia::value>> vec;
    for (auto& v : values)
      vec.emplace_back(v.first, v.second);

    return new Process::ComboBox(
        vec, values[init].second, QString::fromUtf8(name.data(), name.size()), id, parent);
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

  void setup_exec(ossia::value_inlet& v) const { }
};

template <typename ArrT>
struct EnumBase : ossia::safe_nodes::control_in, WidgetFactory::Enum
{
  using type = std::string;
  using port_type = Process::Enum;
  const std::size_t init{};
  const ArrT values;
  using Pixmaps_T = std::array<const char*, 2 * ArrT{}.size()>;
  Pixmaps_T pixmaps;

  const auto& getValues() const { return values; }

  template <std::size_t N1>
  constexpr EnumBase(const char (&name)[N1], std::size_t i, const ArrT& v)
      : ossia::safe_nodes::control_in{name}, init{i}, values{v}, pixmaps{}
  {
  }

  template <std::size_t N1>
  constexpr EnumBase(
      const char (&name)[N1],
      std::size_t i,
      const ArrT& v,
      const Pixmaps_T& pixmaps)
      : ossia::safe_nodes::control_in{name}, init{i}, values{v}, pixmaps{pixmaps}
  {
  }

  ossia::value toValue(std::string v) const { return ossia::value{std::move(v)}; }

  auto create_inlet(Id<Process::Port> id, QObject* parent) const
  {
    return new Process::Enum{
        ossia::flat_set<std::string>(values.begin(), values.end()),
        pixmaps[0] == nullptr ? std::vector<QString>{}
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

  void setup_exec(ossia::value_inlet& v) const { }
};

template <typename ArrT>
struct Enum final : EnumBase<ArrT>
{
  using EnumBase<ArrT>::EnumBase;
  static const constexpr bool must_validate = true;
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

template <typename ArrT>
struct UnvalidatedEnum final : EnumBase<ArrT>
{
  using EnumBase<ArrT>::EnumBase;
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
  return Control::Enum<T2>(t1, s, t2);
}
template <typename T1, typename T2, typename Pixmaps_T>
constexpr auto make_enum(const T1& t1, std::size_t s, const T2& t2, const Pixmaps_T& pixmaps)
{
  return Control::Enum<T2>(t1, s, t2, pixmaps);
}
template <typename T1, typename T2>
constexpr auto make_unvalidated_enum(const T1& t1, std::size_t s, const T2& t2)
{
  return Control::UnvalidatedEnum<T2>(t1, s, t2);
}
/*
template<std::size_t N1, std::size_t N2>
Enum(const char (&name)[N1], std::size_t i, const std::array<const char*, N2>&
v) -> Enum<std::array<const char*, N2>>;
*/

// TODO RGBAEdit
struct RGBAEdit final : ossia::safe_nodes::control_in, WidgetFactory::RGBAEdit
{
  static const constexpr bool must_validate = false;
  using type = std::array<float, 4>;
  std::array<float, 4> init{};
  void setup_exec(ossia::value_inlet& v) const { }
};

// TODO XYZEdit
struct XYZEdit final : ossia::safe_nodes::control_in, WidgetFactory::XYZEdit
{
  static const constexpr bool must_validate = false;
  using type = std::array<float, 3>;
  std::array<float, 3> init{};
  void setup_exec(ossia::value_inlet& v) const { }
};
}
