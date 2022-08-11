#pragma once
#include <Process/Dataflow/Port.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/math.hpp>
#include <ossia/network/domain/domain.hpp>

#include <type_traits>

namespace WidgetFactory
{

struct LinearNormalizer
{
  static constexpr float to01(float min, float range, float val) noexcept
  {
    return (val - min) / range;
  }

  static constexpr float from01(float min, float range, float val) noexcept
  {
    return min + val * range;
  }

  template <typename T>
  static float to01(const T& slider, float val) noexcept
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if(max - min == 0)
      max = min + 1;
    return to01(min, max - min, val);
  }

  template <typename T>
  static float from01(const T& slider, float val) noexcept
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if(max - min == 0)
      max = min + 1;
    return from01(min, max - min, val);
  }
};

struct LogNormalizer
{
  static float to01(float min, float range, float val) noexcept
  {
    return ossia::log_to_normalized(min, range, val);
  }

  static float from01(float min, float range, float val) noexcept
  {
    return ossia::normalized_to_log(min, range, val);
  }

  template <typename T>
  static float to01(const T& slider, float val) noexcept
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if(max - min == 0)
      max = min + 1;
    return to01(min, max - min, val);
  }

  template <typename T>
  static float from01(const T& slider, float val) noexcept
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if(max - min == 0)
      max = min + 1;
    return from01(min, max - min, val);
  }
};

template <typename Norm_T>
struct FixedNormalizer
{
  float min{};
  float max{};
  template <typename T>
  FixedNormalizer(const T& slider)
  {
    min = slider.getMin();
    max = slider.getMax();
    if(max - min == 0)
      max = min + 1;
  }

  constexpr float to01(float val) const noexcept
  {
    return Norm_T::to01(min, max - min, val);
  }

  constexpr float from01(float val) const noexcept
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

  constexpr float to01(float val) const noexcept { return Norm_T::to01(slider, val); }

  constexpr float from01(float val) const noexcept
  {
    return Norm_T::from01(slider, val);
  }
};

template <typename T, typename Control_T, typename Widget_T>
static void bindFloatDomain(const T& slider, Control_T& inlet, Widget_T& widget)
{
  auto min = slider.getMin();
  auto max = slider.getMax();
  if(max - min == 0)
    max = min + 1;

  widget.setRange(min, max);

  if constexpr(std::is_base_of_v<Process::ControlInlet, T>)
  {
    SCORE_ASSERT(&slider == &inlet);
    QObject::connect(&inlet, &Control_T::domainChanged, &widget, [&slider, &widget] {
      auto min = slider.getMin();
      auto max = slider.getMax();
      if(max - min == 0)
        max = min + 1;

      widget.setRange(min, max);
    });
  }
}

template <typename T, typename Control_T, typename Widget_T>
static void bindIntDomain(const T& slider, Control_T& inlet, Widget_T& widget)
{
  auto min = slider.getMin();
  auto max = slider.getMax();
  if(max - min == 0)
    max = min + 1;

  widget.setRange(min, max);

  if constexpr(std::is_base_of_v<Process::ControlInlet, T>)
  {
    SCORE_ASSERT(&slider == &inlet);
    QObject::connect(&inlet, &Control_T::domainChanged, &widget, [&slider, &widget] {
      auto min = slider.getMin();
      auto max = slider.getMax();
      if(max - min == 0)
        max = min + 1;

      widget.setRange(min, max);
    });
  }
}

template <typename T, typename Control_T, typename Widget_T>
static void bindVec2Domain(const T& slider, Control_T& inlet, Widget_T& widget)
{
  auto update_range = [&widget, &inlet] {
    auto min = ossia::get_min(inlet.domain());
    auto max = ossia::get_max(inlet.domain());
    auto min_float = min.template target<float>();
    auto max_float = max.template target<float>();
    if(min_float && max_float)
    {
      if(*max_float - *min_float == 0)
        *max_float = *min_float + 1;
      widget.setRange({*min_float, *min_float}, {*max_float, *max_float});
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

        widget.setRange(min, max);
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
  auto update_range = [&widget, &inlet] {
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

        widget.setRange(min, max);
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
