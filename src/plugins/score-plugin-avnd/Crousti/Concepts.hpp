#pragma once
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/ProcessFlags.hpp>
#include <Process/ProcessMetadata.hpp>

#include <Dataflow/CurveInlet.hpp>
#include <Engine/Node/CommonWidgets.hpp>

#include <score/plugins/UuidKey.hpp>

#include <ossia/dataflow/audio_port.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/safe_nodes/tick_policies.hpp>

#include <boost/container/vector.hpp>

#include <avnd/binding/ossia/dynamic_ports.hpp>
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

#include <span>
#include <type_traits>

#define make_uuid(text) score::uuids::string_generator::compute((text))

namespace oscr
{
template <typename Node, typename FieldIndex>
struct CustomFloatControl;

template <typename BaseInletType, typename Node, typename T, typename FieldIndex>
struct CustomGenericControl;
}

template <typename Node, typename FieldIndex>
struct is_custom_serialized<oscr::CustomFloatControl<Node, FieldIndex>> : std::true_type
{
};

template <typename BaseInletType, typename Node, typename T, typename FieldIndex>
struct is_custom_serialized<
    oscr::CustomGenericControl<BaseInletType, Node, T, FieldIndex>> : std::true_type
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
      : ControlInlet{name, id, parent}
  {
    displayHandledExplicitly = true;
    setValue(init);
    setInit(init);
    setDomain(ossia::make_domain(min, max));
  }

  auto getMin() const noexcept { return domain().get().template convert_min<float>(); }
  auto getMax() const noexcept { return domain().get().template convert_max<float>(); }

  void setupExecution(ossia::inlet& inl, QObject* exec_context) const noexcept override
  {
    auto& port = **safe_cast<ossia::value_inlet*>(&inl);
    port.type = ossia::val_type::FLOAT;
    port.domain = domain().get();
  }
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
};

template <typename BaseInletType, typename Node, typename T, typename FieldIndex>
struct CustomGenericControl : public BaseInletType
{
  using BaseInletType::BaseInletType;

  static BaseInletType::key_type static_concreteKey() noexcept
  {
    return make_field_uuid<Node>(true, FieldIndex{});
  }
  BaseInletType::key_type concreteKey() const noexcept override
  {
    return static_concreteKey();
  }

  void serialize_impl(const VisitorVariant& vis) const noexcept override
  {
    score::serialize_dyn(vis, *this);
  }

  ~CustomGenericControl() = default;
};

template <typename BaseInletType, typename Node, typename T, typename FieldIndex>
using ControlTypeToUse = std::conditional_t<
    avnd::controller_interaction_port<T>,
    CustomGenericControl<BaseInletType, Node, T, FieldIndex>, BaseInletType>;
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

template <typename BaseInletType, typename Node, typename T, typename FieldIndex>
struct TSerializer<
    DataStream, oscr::CustomGenericControl<BaseInletType, Node, T, FieldIndex>>
{
  using model_type = oscr::CustomGenericControl<BaseInletType, Node, T, FieldIndex>;
  static void readFrom(DataStream::Serializer& s, const model_type& p)
  {
    s.read((const BaseInletType&)p);
  }

  static void writeTo(DataStream::Deserializer& s, model_type& eff) { }
};

template <typename BaseInletType, typename Node, typename T, typename FieldIndex>
struct TSerializer<
    JSONObject, oscr::CustomGenericControl<BaseInletType, Node, T, FieldIndex>>
{
  using model_type = oscr::CustomGenericControl<BaseInletType, Node, T, FieldIndex>;
  static void readFrom(JSONObject::Serializer& s, const model_type& p)
  {
    s.read((const BaseInletType&)p);
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
  else if constexpr(widg.widget == avnd::widget_type::folder)
  {
    if constexpr(avnd::has_range<T>) {
      static constexpr auto c = avnd::get_range<T>();
#if defined(_MSC_VER)
      auto init = std::begin(c.init);
      if constexpr(std::is_same_v<std::decay_t<decltype(init)>, const char*>)
        return new Process::FolderChooser{QString::fromUtf8(init, std::size(c.init)), qname, id, parent};
      else
        return new Process::FolderChooser{QString::fromUtf8(c.init.data(), c.init.size()), qname, id, parent};
#else
      return new Process::FolderChooser{
          QString::fromUtf8(&*std::begin(c.init), std::size(c.init)), qname, id, parent};
#endif
    } else {
      return new Process::FolderChooser{"", qname, id, parent};
    }
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
    std::vector<ossia::value> init;
    return new Process::MultiSlider{init, qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::multi_slider_xy)
  {
    std::vector<ossia::value> init;
    return new Process::MultiSliderXY{init, qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::path_generator_xy)
  {
    std::vector<ossia::value> init;
    return new Process::PathGeneratorXY{init, qname, id, parent};
  }
  else if constexpr(widg.widget == avnd::widget_type::spinbox)
  {
    static constexpr auto c = avnd::get_range<T>();
    if constexpr(std::is_integral_v<value_type>)
    {
      return new ControlTypeToUse<Process::IntSpinBox, Node, T, avnd::field_index<N>>{
          c.min, c.max, c.init, qname, id, parent};
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
        return new ControlTypeToUse<Process::LineEdit, Node, T, avnd::field_index<N>>{
            c.init.data(), qname, id, parent};
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
    using init_type = std::decay_t<decltype(c.init)>;
    ossia::value init;
    if constexpr(std::is_integral_v<init_type> || std::is_enum_v<init_type>)
      init = static_cast<int>(c.init);
    else
      init = c.init;
    return new Process::ComboBox{
        to_combobox_range(c.values), std::move(init), qname, id, parent};
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
          false,
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
    return new Process::ValueInlet{qname, id, parent};
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
    return new Process::ValueOutlet{qname, id, parent};
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
