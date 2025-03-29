#pragma once
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/ProcessFlags.hpp>
#include <Process/ProcessMetadata.hpp>

#include <Dataflow/CurveInlet.hpp>

#include <score/plugins/UuidKey.hpp>

#include <ossia/dataflow/audio_port.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/safe_nodes/tick_policies.hpp>
#include <ossia/detail/span.hpp>

#include <boost/container/vector.hpp>

#include <avnd/binding/ossia/qt.hpp>
#include <avnd/binding/ossia/uuid.hpp>
#include <avnd/common/concepts_polyfill.hpp>
#include <avnd/common/struct_reflection.hpp>
#include <avnd/concepts/audio_port.hpp>
#include <avnd/concepts/channels.hpp>
#include <avnd/concepts/curve.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/concepts/midi_port.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/wrappers/metadatas.hpp>
#include <avnd/wrappers/widgets.hpp>

#include <cmath>

#include <type_traits>

#define make_uuid(text) score::uuids::string_generator::compute((text))

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

struct CustomFloatControlBase : public Process::ControlInlet
{
  using Process::ControlInlet::ControlInlet;

  CustomFloatControlBase(
      float min, float max, float init, const QString& name, Id<Process::Port> id,
      QObject* parent)
      : ControlInlet{id, parent}
  {
    hidden = true;
    setValue(init);
    setInit(init);
    setDomain(ossia::make_domain(min, max));
    setName(name);
  }

  auto getMin() const noexcept { return domain().get().template convert_min<float>(); }
  auto getMax() const noexcept { return domain().get().template convert_max<float>(); }
};

template <typename Node, typename FieldIndex>
struct CustomFloatControl : public CustomFloatControlBase
{
  using CustomFloatControlBase::CustomFloatControlBase;

  static key_type static_concreteKey() noexcept
  {
    return make_field_uuid<Node>(true, FieldIndex{});
  }
  key_type concreteKey() const noexcept override { return static_concreteKey(); }

  void serialize_impl(const VisitorVariant& vis) const noexcept override
  {
    score::serialize_dyn(vis, *this);
  }

  ~CustomFloatControl() = default;

  void setupExecution(ossia::inlet& inl) const noexcept override
  {
    auto& port = **safe_cast<ossia::value_inlet*>(&inl);
    port.type = ossia::val_type::FLOAT;
    using inlet_type = avnd::input_introspection<Node>;
    using port_type = inlet_type::template field_type<FieldIndex{}>;
    if constexpr(avnd::has_range<port_type>)
    {
      static constexpr auto range = avnd::get_range<port_type>();
      ossia::domain_base<float> dom;
      dom.min = range.min;
      dom.max = range.max;
      port.domain = dom;
    }
  }
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

template <typename Node, typename T, std::size_t N>
static inline auto
make_control_in(avnd::field_index<N>, Id<Process::Port>&& id, QObject* parent)
{
  using value_type = as_type(T::value);
  constexpr auto name = avnd::get_name<T>();
  QString qname = QString::fromUtf8(name.data(), name.size());

  constexpr auto widg = avnd::get_widget<T>();

  // FIXME log normalization & friends

  if constexpr(avnd::curve_port<T>)
  {
    return new Dataflow::CurveInlet(id, parent);
  }
  else if constexpr(widg.widget == avnd::widget_type::bang)
  {
    return new Process::ImpulseButton{qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::button)
  {
    return new Process::Button{qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::toggle)
  {
    static constexpr auto c = avnd::get_range<T>();
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
    static constexpr auto c = avnd::get_range<T>();
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
  else if constexpr(widg.widget == avnd::widget_type::log_slider)
  {
    static constexpr auto c = avnd::get_range<T>();
    return new Process::LogFloatSlider{c.min, c.max, c.init, qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::log_knob)
  {
    // FIXME
    static constexpr auto c = avnd::get_range<T>();
    return new Process::LogFloatSlider{c.min, c.max, c.init, qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::time_chooser)
  {
    static constexpr auto c = avnd::get_range<T>();
    return new Process::TimeChooser{c.min, c.max, c.init, qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::range_slider)
  {
    static constexpr auto c = avnd::get_range<T>();
    if constexpr(std::is_integral_v<value_type>)
    {
      auto [start, end] = c.init;
      return new Process::IntRangeSlider{c.min, c.max, {(float)start, (float)end},
                                         qname, id,    parent};
    }
    else
    {
      auto [start, end] = c.init;
      return new Process::FloatRangeSlider{c.min, c.max, {(float)start, (float)end},
                                           qname, id,    parent};
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::range_spinbox)
  {
    static constexpr auto c = avnd::get_range<T>();
    if constexpr(std::is_integral_v<value_type>)
    {
      auto [start, end] = c.init;
      return new Process::IntRangeSpinBox{c.min, c.max, {(float)start, (float)end},
                                          qname, id,    parent};
    }
    else
    {
      auto [start, end] = c.init;
      return new Process::FloatRangeSpinBox{c.min, c.max, {(float)start, (float)end},
                                            qname, id,    parent};
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::multi_slider)
  {
    constexpr auto c = avnd::get_range<T>();
    auto [start, end] = c.init;
    std::vector<ossia::value> init;
    return new Process::MultiSlider{init, qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::spinbox)
  {
    static constexpr auto c = avnd::get_range<T>();
    if constexpr(std::is_integral_v<value_type>)
    {
      return new Process::IntSpinBox{c.min, c.max, c.init, qname, id, parent};
    }
    else
    {
      return new Process::FloatSpinBox{c.min, c.max, c.init, qname, id, parent};
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::knob)
  {
    static constexpr auto c = avnd::get_range<T>();
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
    if constexpr(avnd::has_range<T>)
    {
      static constexpr auto c = avnd::get_range<T>();
      if constexpr(avnd::program_parameter<T>)
      {
        auto p = new Process::ProgramEdit{c.init.data(), qname, id, parent};
        p->language = T::language();
        return p;
      }
      else
        return new Process::LineEdit{c.init.data(), qname, id, parent};
    }
    else
    {
      if constexpr(avnd::program_parameter<T>)
      {
        auto p = new Process::ProgramEdit{"", qname, id, parent};
        p->language = T::language();
        return p;
      }
      else
        return new Process::LineEdit{"", qname, id, parent};
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::combobox)
  {
    static constexpr auto c = avnd::get_range<T>();
    return new Process::ComboBox{to_combobox_range(c.values), c.init, qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::choices)
  {
    static constexpr auto c = avnd::get_range<T>();
    auto enums = avnd::to_enum_range(c.values);
    static_assert(
        std::is_integral_v<decltype(c.init)> || std::is_enum_v<decltype(c.init)>);
    auto init = enums[static_cast<int>(c.init)];
    std::vector<QString> pixmaps;
    if constexpr(avnd::has_pixmaps<T>)
    {
      for(std::string_view pix : avnd::get_pixmaps<T>())
      {
        pixmaps.push_back(QString::fromLatin1(pix.data(), pix.size()));
      }
    }
    return new Process::Enum{
        std::move(enums), pixmaps, std::move(init), qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::xy)
  {
    static constexpr auto c = avnd::get_range<T>();
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
  else if constexpr(widg.widget == avnd::widget_type::xyz)
  {
    static constexpr auto c = avnd::get_range<T>();
    if constexpr(requires {
                   c.min == 0.f;
                   c.max == 0.f;
                   c.init == 0.f;
                 })
    {
      return new Process::XYZSlider{
          {c.min, c.min, c.min},
          {c.max, c.max, c.max},
          {c.init, c.init, c.init},
          qname,
          id,
          parent};
    }
    else
    {
      auto [mx, my, mz] = c.min;
      auto [Mx, My, Mz] = c.max;
      auto [ix, iy, iz] = c.init;
      return new Process::XYZSlider{{mx, my, mz}, {Mx, My, Mz}, {ix, iy, iz},
                                    qname,        id,           parent};
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::xy_spinbox)
  {
    using data_type = std::decay_t<decltype(value_type{}.x)>;
    static constexpr auto c = avnd::get_range<T>();
    if constexpr(requires {
                   c.min == 0.f;
                   c.max == 0.f;
                   c.init == 0.f;
                 })
    {
      return new Process::XYSpinboxes{
          {c.min, c.min},
          {c.max, c.max},
          {c.init, c.init},
          std::is_integral_v<data_type>,
          qname,
          id,
          parent};
    }
    else
    {
      auto [mx, my] = c.min;
      auto [Mx, My] = c.max;
      auto [ix, iy] = c.init;
      return new Process::XYSpinboxes{
          {mx, my}, {Mx, My}, {ix, iy}, std::is_integral_v<data_type>,
          qname,    id,       parent};
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::xyz_spinbox)
  {
    static constexpr auto c = avnd::get_range<T>();
    if constexpr(requires {
                   c.min == 0.f;
                   c.max == 0.f;
                   c.init == 0.f;
                 })
    {
      return new Process::XYZSpinboxes{
          {c.min, c.min, c.min},
          {c.max, c.max, c.max},
          {c.init, c.init, c.init},
          qname,
          id,
          parent};
    }
    else
    {
      auto [mx, my, mz] = c.min;
      auto [Mx, My, Mz] = c.max;
      auto [ix, iy, iz] = c.init;
      return new Process::XYZSpinboxes{{mx, my, mz}, {Mx, My, Mz}, {ix, iy, iz},
                                       qname,        id,           parent};
    }
  }
  else if constexpr(widg.widget == avnd::widget_type::color)
  {
    static constexpr auto c = avnd::get_range<T>();
    static constexpr auto i = c.init;
    return new Process::HSVSlider{{i.r, i.g, i.b, i.a}, qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::control)
  {
    return new Process::ControlInlet{qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::no_control)
  {
    auto pv = new Process::ValueInlet{id, parent};
    pv->setName(qname);
    return pv;
  }
  else
  {
    static_assert(T::is_not_a_valid_control);
  }
}

template <typename T, std::size_t N>
static inline auto
make_control_out(avnd::field_index<N>, Id<Process::Port>&& id, QObject* parent)
{
  constexpr auto name = avnd::get_name<T>();
  constexpr auto widg = avnd::get_widget<T>();
  QString qname = QString::fromUtf8(name.data(), name.size());

  // FIXME log normalization & friends

  if constexpr(widg.widget == avnd::widget_type::bargraph)
  {
    static constexpr auto c = avnd::get_range<T>();
    return new Process::Bargraph{c.min, c.max, c.init, qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::control)
  {
    return new Process::ControlOutlet{qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::no_control)
  {
    auto pv = new Process::ValueOutlet{id, parent};
    pv->setName(qname);
    return pv;
  }
  else if constexpr(avnd::fp_ish<decltype(T::value)>)
  {
    static constexpr auto c = avnd::get_range<T>();
    return new Process::Bargraph{c.min, c.max, c.init, qname, id, parent};
  }
  else
  {
    return new Process::ControlOutlet{qname, id, parent};
  }
}

template <typename T>
static inline constexpr auto make_control_out(const T& t)
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

}
