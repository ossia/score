#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <ossia/editor/curve/curve_segment/easing.hpp>
#include <ossia/network/value/value.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <cmath>

namespace ao
{
/**
 * @brief A collection of easing behaviours.
 * 
 * Note: uses the ossia collection of easing functions, 
 * but those are pretty self-contained and could be ported 
 * out of their current folder without issue.
 */
struct Easetanbul
{
public:
  halp_meta(name, "Easetanbul")
  halp_meta(c_name, "easetanbul")
  halp_meta(category, "Control/Mappings")
  halp_meta(description, "Easing function power tool")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/easetanbul.html")
  halp_meta(author, "Jean-Michaël Celerier")
  halp_meta(uuid, "8abb3061-7101-48d4-81b1-6b19208098bf")

  enum Mode
  {
    Linear,
    QuadraticIn,
    QuadraticOut,
    QuadraticInOut,
    CubicIn,
    CubicOut,
    CubicInOut,
    QuarticIn,
    QuarticOut,
    QuarticInOut,
    QuinticIn,
    QuinticOut,
    QuinticInOut,
    SineIn,
    SineOut,
    SineInOut,
    CircularIn,
    CircularOut,
    CircularInOut,
    ExponentialIn,
    ExponentialOut,
    ExponentialInOut,
    ElasticIn,
    ElasticOut,
    ElasticInOut,
    BackIn,
    BackOut,
    BackInOut,
    BounceIn,
    BounceOut,
    BounceInOut,
    PerlinInOut,
    Kinematic,
    PID
  };

  // Kinematic:
  // https://jcgt.org/published/0011/03/02/paper.pdf

  // PID:
  /* https://news.ycombinator.com/item?id=44584830
   *    function sprung_response(t,pos,vel,k,c,m)
      local decay = c/2/m
      local omega = math.sqrt(k/m)
      local resid = decay*decay-omega*omega
      local scale = math.sqrt(math.abs(resid))
      local T1,T0 = t , 1
      if resid<0 then
         T1,T0 = math.sin( scale*t)/scale , math.cos( scale*t)
      elseif resid>0 then
         T1,T0 = math.sinh(scale*t)/scale , math.cosh(scale*t)
      end
      local dissipation = math.exp(-decay*t)
      local evolved_pos = dissipation*( pos*(T0+T1*decay) + vel*(   T1      ) )
      local evolved_vel = dissipation*( pos*(-T1*omega^2) + vel*(T0-T1*decay) )
     return evolved_pos , evolved_vel
   end
*/
  struct
  {
    struct : halp::val_port<"Input", ossia::value>
    {
      void update(Easetanbul& self)
      {
        self.m_previous = self.m_current;
        self.m_current = value;
        self.m_running = true;
        self.m_elapsed = 0;
      }
    } value;
    struct : halp::enum_t<Mode, "Mode">
    {
      enum widget
      {
        combobox
      };
    } ease;
    struct : halp::time_chooser<"Delay", halp::range{0.001, 30., 0.2}>
    {
      void update(Easetanbul& self) { self.m_duration = value * self.rate; }
    } delay;
    // retrigger on each value vs interpolate ?

  } inputs;

  struct
  {
    halp::val_port<"Output", ossia::value> output;
  } outputs;

  double rate = 48000.;
  void prepare(halp::setup info) noexcept { rate = info.rate; }

  double ease01(double value)
  {
    using ease_type = Mode;
    switch(inputs.ease)
    {
      default:
      case ease_type::Linear:
        return ossia::easing::linear{}(value);
      case ease_type::BackIn:
        return ossia::easing::backIn{}(value);
      case ease_type::BackOut:
        return ossia::easing::backOut{}(value);
      case ease_type::BackInOut:
        return ossia::easing::backInOut{}(value);
      case ease_type::BounceIn:
        return ossia::easing::bounceIn{}(value);
      case ease_type::BounceOut:
        return ossia::easing::bounceOut{}(value);
      case ease_type::BounceInOut:
        return ossia::easing::bounceInOut{}(value);
      case ease_type::QuadraticIn:
        return ossia::easing::quadraticIn{}(value);
      case ease_type::QuadraticOut:
        return ossia::easing::quadraticOut{}(value);
      case ease_type::QuadraticInOut:
        return ossia::easing::quadraticInOut{}(value);
      case ease_type::CubicIn:
        return ossia::easing::cubicIn{}(value);
      case ease_type::CubicOut:
        return ossia::easing::cubicOut{}(value);
      case ease_type::CubicInOut:
        return ossia::easing::cubicInOut{}(value);
      case ease_type::QuarticIn:
        return ossia::easing::quarticIn{}(value);
      case ease_type::QuarticOut:
        return ossia::easing::quarticOut{}(value);
      case ease_type::QuarticInOut:
        return ossia::easing::quarticInOut{}(value);
      case ease_type::QuinticIn:
        return ossia::easing::quinticIn{}(value);
      case ease_type::QuinticOut:
        return ossia::easing::quinticOut{}(value);
      case ease_type::QuinticInOut:
        return ossia::easing::quinticInOut{}(value);
      case ease_type::SineIn:
        return ossia::easing::sineIn{}(value);
      case ease_type::SineOut:
        return ossia::easing::sineOut{}(value);
      case ease_type::SineInOut:
        return ossia::easing::sineInOut{}(value);
      case ease_type::CircularIn:
        return ossia::easing::circularIn{}(value);
      case ease_type::CircularOut:
        return ossia::easing::circularOut{}(value);
      case ease_type::CircularInOut:
        return ossia::easing::circularInOut{}(value);
      case ease_type::ExponentialIn:
        return ossia::easing::exponentialIn{}(value);
      case ease_type::ExponentialOut:
        return ossia::easing::exponentialOut{}(value);
      case ease_type::ExponentialInOut:
        return ossia::easing::exponentialInOut{}(value);
      case ease_type::ElasticIn:
        return ossia::easing::elasticIn{}(value);
      case ease_type::ElasticOut:
        return ossia::easing::elasticOut{}(value);
      case ease_type::ElasticInOut:
        return ossia::easing::elasticInOut{}(value);
      case ease_type::PerlinInOut:
        return ossia::easing::perlinInOut{}(value);
    }
  }

  template <typename T>
  static constexpr T ease_scalar(T v0, T v1, float t)
  {
    return ossia::easing::ease{}(v0, v1, t);
  }

  template <std::size_t N>
  static constexpr std::array<float, N>
  ease_vector(const std::array<float, N>& v0, const std::array<float, N>& v1, float t)
  {
    std::array<float, N> res;
    for(int i = 0; i < N; i++)
      res[i] = ossia::easing::ease{}(v0[i], v1[i], t);
    return res;
  }

  static std::vector<ossia::value> ease_vector(
      const std::vector<ossia::value>& v0, const std::vector<ossia::value>& v1, float t)
  {
    const int max_size = std::min(v0.size(), v1.size());

    std::vector<ossia::value> result;
    result.reserve(max_size);

    for(int elem_idx = 0; elem_idx < max_size; ++elem_idx)
      result.push_back(ease_values(v0[elem_idx], v1[elem_idx], t));

    return result;
  }

  static ossia::value
  ease_values(const ossia::value& v0, const ossia::value& v1, float weights)
  {
    if(!v0.valid())
      return v1;
    if(!v1.valid())
      return v0;
    const auto first_type = v0.get_type();
    const auto second_type = v1.get_type();
    const bool same_type = first_type == second_type;

    if(!same_type)
      return v0;

    switch(first_type)
    {
      case ossia::val_type::FLOAT: {
        return ease_scalar(*v0.target<float>(), *v1.target<float>(), weights);
      }
      case ossia::val_type::INT: {
        return static_cast<int>(
            std::round(ease_scalar(*v0.target<int>(), *v1.target<int>(), weights)));
      }
      case ossia::val_type::BOOL: {
        return ease_scalar(*v0.target<bool>(), *v1.target<bool>(), weights) >= 0.5f;
      }
      case ossia::val_type::VEC2F: {
        return ease_vector(
            *v0.target<ossia::vec2f>(), *v1.target<ossia::vec2f>(), weights);
      }
      case ossia::val_type::VEC3F: {
        return ease_vector(
            *v0.target<ossia::vec3f>(), *v1.target<ossia::vec3f>(), weights);
      }
      case ossia::val_type::VEC4F: {
        return ease_vector(
            *v0.target<ossia::vec4f>(), *v1.target<ossia::vec4f>(), weights);
      }
      case ossia::val_type::LIST: {
        return ease_vector(
            *v0.target<std::vector<ossia::value>>(),
            *v1.target<std::vector<ossia::value>>(), weights);
      }
      default:
        return v0;
    }
  }

  void operator()(int frames) noexcept
  {
    double deltaTime = frames;
    if(!m_running)
    {
      outputs.output = m_current;
      return;
    }

    m_elapsed += deltaTime;

    if(m_elapsed >= m_duration)
    {
      outputs.output = m_current;
      m_running = false;
    }
    else
    {
      double t = m_elapsed / m_duration;
      double easedT = ease01(t);
      outputs.output = ease_values(m_previous, m_current, easedT);
    }
  }

  ossia::value m_previous{};
  ossia::value m_current{};
  double m_duration{96000.};
  double m_elapsed{};
  bool m_running{};
};
}
