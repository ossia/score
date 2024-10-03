#pragma once
#include <ossia/detail/pod_vector.hpp>
#include <ossia/detail/small_vector.hpp>
#include <ossia/detail/variant.hpp>

#include <halp/audio.hpp>
#include <halp/callback.hpp>
#include <halp/controls.hpp>
#include <halp/mappers.hpp>

#include <array>
namespace A2
{
static constexpr auto multichannel_max_count = 8;
using analysis_vector = ossia::small_pod_vector<float, multichannel_max_count>;
using output_type = ossia::variant<float, std::array<float, 2>, analysis_vector>;

struct value_out : halp::val_port<"out", output_type>
{
};

struct pulse_out : halp::callback<"pulse">
{
};

struct audio_in : halp::dynamic_audio_bus<"in", double>
{
};
struct audio_out : halp::dynamic_audio_bus<"out", double>
{
};

struct gain_slider : halp::hslider_f32<"Gain", halp::range{0., 100., 1.}>
{
  using mapper = halp::log_mapper<std::ratio<95, 100>>;
};

struct gate_slider : halp::hslider_f32<"Gate", halp::range{0., 1., 0.}>
{
};
}
