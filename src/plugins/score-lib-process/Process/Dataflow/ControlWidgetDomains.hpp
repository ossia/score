#pragma once
#include <Process/Dataflow/Port.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/math.hpp>
#include <ossia/network/domain/domain.hpp>

#include <QDoubleSpinBox>
#include <QSpinBox>

#include <type_traits>

namespace WidgetFactory
{

template <typename T>
static constexpr auto getMin(auto& slider) noexcept
{
  if constexpr(requires { slider.getMin(); })
    return slider.getMin();
  else
    return slider.domain().get().template convert_min<T>();
}

template <typename T>
static constexpr auto getMax(auto& slider) noexcept
{
  if constexpr(requires { slider.getMax(); })
    return slider.getMax();
  else
    return slider.domain().get().template convert_max<T>();
}

template <typename T>
static constexpr auto getInit(auto& slider) noexcept
{
  if constexpr(requires { slider.getInit(); })
    return slider.getInit();
  else if constexpr(requires { slider.init(); })
    return ossia::convert<T>(slider.init());
  else
    return getMin<T>(slider);
}

struct LinearNormalizer
{
  static constexpr double to01(double min, double range, double val) noexcept
  {
    return (val - min) / range;
  }

  static constexpr double from01(double min, double range, double val) noexcept
  {
    return min + val * range;
  }

  template <typename T>
  static double to01(const T& slider, double val) noexcept
  {
    auto min = getMin<double>(slider);
    auto max = getMax<double>(slider);
    if(max - min == 0)
      max = min + 1;
    return to01(min, max - min, val);
  }

  template <typename T>
  static double from01(const T& slider, double val) noexcept
  {
    auto min = getMin<double>(slider);
    auto max = getMax<double>(slider);
    if(max - min == 0)
      max = min + 1;
    return from01(min, max - min, val);
  }

  template <typename T>
  static std::array<double, 2> to01(const T& slider, ossia::vec2f val) noexcept
  {
    std::array<double, 2> res;
    const auto min = getMin<ossia::vec2f>(slider);
    const auto max = getMax<ossia::vec2f>(slider);
    res[0] = to01(min[0], double(max[0]) - double(min[0]), val[0]);
    res[1] = to01(min[1], double(max[1]) - double(min[1]), val[1]);
    return res;
  }

  template <typename T>
  static ossia::vec2f from01(const T& slider, std::array<double, 2> val) noexcept
  {
    ossia::vec2f res;
    const auto min = getMin<ossia::vec2f>(slider);
    const auto max = getMax<ossia::vec2f>(slider);
    res[0] = from01(min[0], double(max[0]) - double(min[0]), val[0]);
    res[1] = from01(min[1], double(max[1]) - double(min[1]), val[1]);
    return res;
  }

  template <typename T>
  static std::array<double, 3> to01(const T& slider, ossia::vec3f val) noexcept
  {
    std::array<double, 3> res;
    const auto min = getMin<ossia::vec3f>(slider);
    const auto max = getMax<ossia::vec3f>(slider);
    res[0] = to01(min[0], double(max[0]) - double(min[0]), val[0]);
    res[1] = to01(min[1], double(max[1]) - double(min[1]), val[1]);
    res[2] = to01(min[2], double(max[2]) - double(min[2]), val[2]);
    return res;
  }

  template <typename T>
  static ossia::vec3f from01(const T& slider, std::array<double, 3> val) noexcept
  {
    ossia::vec3f res;
    const auto min = getMin<ossia::vec3f>(slider);
    const auto max = getMax<ossia::vec3f>(slider);
    res[0] = from01(min[0], double(max[0]) - double(min[0]), val[0]);
    res[1] = from01(min[1], double(max[1]) - double(min[1]), val[1]);
    res[2] = from01(min[2], double(max[2]) - double(min[2]), val[2]);
    return res;
  }

  template <typename T>
  static std::array<double, 4> to01(const T& slider, ossia::vec4f val) noexcept
  {
    std::array<double, 4> res;
    const auto min = getMin<ossia::vec4f>(slider);
    const auto max = getMax<ossia::vec4f>(slider);
    res[0] = to01(min[0], double(max[0]) - double(min[0]), val[0]);
    res[1] = to01(min[1], double(max[1]) - double(min[1]), val[1]);
    res[2] = to01(min[2], double(max[2]) - double(min[2]), val[2]);
    res[3] = to01(min[3], double(max[3]) - double(min[3]), val[3]);
    return res;
  }

  template <typename T>
  static ossia::vec4f from01(const T& slider, std::array<double, 4> val) noexcept
  {
    ossia::vec4f res;
    const auto min = getMin<ossia::vec4f>(slider);
    const auto max = getMax<ossia::vec4f>(slider);
    res[0] = from01(min[0], double(max[0]) - double(min[0]), val[0]);
    res[1] = from01(min[1], double(max[1]) - double(min[1]), val[1]);
    res[2] = from01(min[2], double(max[2]) - double(min[2]), val[2]);
    res[3] = from01(min[3], double(max[3]) - double(min[3]), val[3]);
    return res;
  }

  template <typename T, std::size_t N>
  static std::array<float, N> from01(const T& slider, std::array<double, N> val) noexcept
  {
    std::array<float, N> res;
    const auto min = getMin<std::array<double, N>>(slider);
    const auto max = getMax<std::array<double, N>>(slider);
    for(std::size_t i = 0; i < N; i++)
      res[i] = from01(min[i], double(max[i]) - double(min[i]), val[i]);
    return res;
  }
};

struct LogNormalizer
{
  static double to01(double min, double range, double val) noexcept
  {
    return ossia::log_to_normalized(min, range, val);
  }

  static double from01(double min, double range, double val) noexcept
  {
    return ossia::normalized_to_log(min, range, val);
  }

  template <typename T>
  static double to01(const T& slider, double val) noexcept
  {
    auto min = getMin<double>(slider);
    auto max = getMax<double>(slider);
    if(max - min == 0)
      max = min + 1;
    return to01(min, max - min, val);
  }

  template <typename T>
  static double from01(const T& slider, double val) noexcept
  {
    auto min = getMin<double>(slider);
    auto max = getMax<double>(slider);
    if(max - min == 0)
      max = min + 1;
    return from01(min, max - min, val);
  }
};

template <typename Norm_T>
struct FixedNormalizer
{
  double min{};
  double max{};
  template <typename T>
  FixedNormalizer(const T& slider)
  {
    min = getMin<double>(slider);
    max = getMax<double>(slider);
    if(max - min == 0)
      max = min + 1;
  }

  constexpr double to01(double val) const noexcept
  {
    return Norm_T::to01(min, max - min, val);
  }

  constexpr double from01(double val) const noexcept
  {
    return Norm_T::from01(min, max - min, val);
  }
};

template <typename Norm_T, typename Slider_T>
struct UpdatingNormalizer
{
  const Slider_T& slider;

  UpdatingNormalizer(const Slider_T& sl)
      : slider{sl}
  {
  }

  constexpr double to01(double val) const noexcept { return Norm_T::to01(slider, val); }

  constexpr double from01(double val) const noexcept
  {
    return Norm_T::from01(slider, val);
  }
};

template <typename Control_T, typename Widget_T>
static void initWidgetProperties(Control_T& inlet, Widget_T& widget)
{
  if constexpr(requires { widget.setNoValueChangeOnMove(true); })
    widget.setNoValueChangeOnMove(inlet.noValueChangeOnMove);
}

template <typename Widget_T>
static void setRange(Widget_T& widget, auto&& min, auto&& max, auto&& init)
{
  widget.setRange(min, max, init);
}

static inline void setRange(QSpinBox& widget, auto&& min, auto&& max, auto&& init)
{
  widget.setRange(min, max);
}

static inline void setRange(QDoubleSpinBox& widget, auto&& min, auto&& max, auto&& init)
{
  widget.setRange(min, max);
}

template <typename T, typename Control_T, typename Widget_T>
static void bindFloatDomain(const T& slider, Control_T& inlet, Widget_T& widget)
{
  auto min = getMin<float>(slider);
  auto max = getMax<float>(slider);
  if(max - min == 0)
    max = min + 1;

  setRange(widget, min, max, getInit<float>(slider));

  if constexpr(std::is_base_of_v<Process::ControlInlet, T>)
  {
    SCORE_ASSERT(&slider == &inlet);
    QObject::connect(&inlet, &Control_T::domainChanged, &widget, [&slider, &widget] {
      auto min = getMin<float>(slider);
      auto max = getMax<float>(slider);
      if(max - min == 0)
        max = min + 1;

      setRange(widget, min, max, getInit<float>(slider));
    });
  }
}

template <typename T, typename Control_T, typename Widget_T>
static void bindIntDomain(const T& slider, Control_T& inlet, Widget_T& widget)
{
  auto min = getMin<int>(slider);
  auto max = getMax<int>(slider);
  if(max - min == 0)
    max = min + 1;

  setRange(widget, min, max, getInit<int>(slider));

  if constexpr(std::is_base_of_v<Process::ControlInlet, T>)
  {
    SCORE_ASSERT(&slider == &inlet);
    QObject::connect(&inlet, &Control_T::domainChanged, &widget, [&slider, &widget] {
      auto min = getMin<int>(slider);
      auto max = getMax<int>(slider);
      if(max - min == 0)
        max = min + 1;

      setRange(widget, min, max, getInit<int>(slider));
    });
  }
}

template <typename T, typename Control_T, typename Widget_T>
static void bindVec2Domain(const T& slider, Control_T& inlet, Widget_T& widget)
{
  auto update_range = [&widget, &inlet, &slider] {
    auto min = ossia::get_min(inlet.domain());
    auto max = ossia::get_max(inlet.domain());
    auto min_float = min.template target<float>();
    auto max_float = max.template target<float>();
    if(min_float && max_float)
    {
      if(*max_float - *min_float == 0)
        *max_float = *min_float + 1;
      widget.setRange(
          {*min_float, *min_float}, {*max_float, *max_float}, {*min_float, *min_float});
    }
    else
    {
      auto min_vec2 = min.template target<ossia::vec2f>();
      auto max_vec2 = max.template target<ossia::vec2f>();
      if(min_vec2 && max_vec2)
      {
        auto& min = *min_vec2;
        auto& max = *max_vec2;
        for(std::size_t i = 0; i < min.size(); i++)
        {
          if(max[i] - min[i] == 0)
            max[i] = min[i] + 1;
        }

        auto init_vec2 = getInit<ossia::vec2f>(slider);
        widget.setRange(min, max, init_vec2);
      }
    }
  };

  update_range();

  if constexpr(std::is_base_of_v<Process::ControlInlet, T>)
  {
    SCORE_ASSERT(&slider == &inlet);
    QObject::connect(&inlet, &Control_T::domainChanged, &widget, update_range);
  }
}

template <typename T, typename Control_T, typename Widget_T>
static void bindVec3Domain(const T& slider, Control_T& inlet, Widget_T& widget)
{
  auto update_range = [&widget, &inlet, &slider] {
    auto min = ossia::get_min(inlet.domain());
    auto max = ossia::get_max(inlet.domain());
    auto min_float = min.template target<float>();
    auto max_float = max.template target<float>();
    if(min_float && max_float)
    {
      if(*max_float - *min_float == 0)
        *max_float = *min_float + 1;
      widget.setRange(
          {*min_float, *min_float, *min_float}, {*max_float, *max_float, *max_float});
    }
    else
    {
      auto min_vec3 = min.template target<ossia::vec3f>();
      auto max_vec3 = max.template target<ossia::vec3f>();
      if(min_vec3 && max_vec3)
      {
        auto& min = *min_vec3;
        auto& max = *max_vec3;
        for(std::size_t i = 0; i < min.size(); i++)
        {
          if(max[i] - min[i] == 0)
            max[i] = min[i] + 1;
        }

        auto init_vec3 = getInit<ossia::vec3f>(slider);
        widget.setRange(min, max, init_vec3);
      }
    }
  };

  update_range();

  if constexpr(std::is_base_of_v<Process::ControlInlet, T>)
  {
    SCORE_ASSERT(&slider == &inlet);
    QObject::connect(&inlet, &Control_T::domainChanged, &widget, update_range);
  }
}
}
