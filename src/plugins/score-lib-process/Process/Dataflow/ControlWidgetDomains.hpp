#pragma once
#include <Process/Dataflow/Port.hpp>

#include <score/tools/Debug.hpp>
#include <ossia/detail/math.hpp>

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

  template<typename T>
  static float to01(const T& slider, float val) noexcept
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if (max - min == 0)
      max = min + 1;
    return to01(min, max - min, val);
  }

  template<typename T>
  static float from01(const T& slider, float val) noexcept
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if (max - min == 0)
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

  template<typename T>
  static float to01(const T& slider, float val) noexcept
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if (max - min == 0)
      max = min + 1;
    return to01(min, max - min, val);
  }

  template<typename T>
  static float from01(const T& slider, float val) noexcept
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if (max - min == 0)
      max = min + 1;
    return from01(min, max - min, val);
  }
};

template<typename Norm_T>
struct FixedNormalizer {
  float min{};
  float max{};
  template<typename T>
  FixedNormalizer(const T& slider) {
    min = slider.getMin();
    max = slider.getMax();
    if (max - min == 0)
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

template<typename Norm_T, typename Slider_T>
struct UpdatingNormalizer {
  const Slider_T& slider;

  UpdatingNormalizer(const Slider_T& sl)
    : slider{sl}
  {
  }

  constexpr float to01(float val) const noexcept
  {
    return Norm_T::to01(slider, val);
  }

  constexpr float from01(float val) const noexcept
  {
    return Norm_T::from01(slider, val);
  }
};

template<typename T, typename Control_T, typename Widget_T>
static void bindFloatDomain(const T& slider, Control_T& inlet, Widget_T& widget)
{
  auto min = slider.getMin();
  auto max = slider.getMax();
  if (max - min == 0)
    max = min + 1;

  widget.setRange(min, max);

  if constexpr(std::is_base_of_v<Process::ControlInlet, T>)
  {
    SCORE_ASSERT(&slider == &inlet);
    QObject::connect(&inlet, &Control_T::domainChanged,
                     &widget, [&slider, &widget] {
      auto min = slider.getMin();
      auto max = slider.getMax();
      if (max - min == 0)
        max = min + 1;

      widget.setRange(min, max);
    });
  }
}


template<typename T, typename Control_T, typename Widget_T>
static void bindIntDomain(const T& slider, Control_T& inlet, Widget_T& widget)
{
  auto min = slider.getMin();
  auto max = slider.getMax();
  if (max - min == 0)
    max = min + 1;

  widget.setRange(min, max);

  if constexpr(std::is_base_of_v<Process::ControlInlet, T>)
  {
    SCORE_ASSERT(&slider == &inlet);
    QObject::connect(&inlet, &Control_T::domainChanged,
                     &widget, [&slider, &widget] {
      auto min = slider.getMin();
      auto max = slider.getMax();
      if (max - min == 0)
        max = min + 1;

      widget.setRange(min, max);
    });
  }
}


}
