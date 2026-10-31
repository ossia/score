#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <optional>

namespace Threedim
{

//! 1 euro filter, Casiez, Roussel & Vogel 2012 (doi:10.1145/2207676.2208639),
//! as in the authors' reference implementation (github.com/casiez/OneEuroFilter).
struct OneEuroFilter
{
  double minCutoff{1.}, beta{0.}, dCutoff{1.};
  bool primed{};
  double x{}, dx{}, t{};

  static double alpha(double cutoff, double te) noexcept
  {
    const double tau = 1. / (2. * M_PI * cutoff);
    return 1. / (1. + tau / te);
  }
  void reset() noexcept { primed = false; }
  double operator()(double value, double timeSeconds) noexcept
  {
    if(!primed)
    {
      primed = true;
      x = value;
      dx = 0.;
      t = timeSeconds;
      return x;
    }
    const double te = timeSeconds - t;
    if(!(te > 0.))
      return x;
    t = timeSeconds;
    const double d = (value - x) / te;
    dx += alpha(dCutoff, te) * (d - dx);
    const double cutoff = minCutoff + beta * std::abs(dx);
    x += alpha(cutoff, te) * (value - x);
    return x;
  }
};

class ViewerFromPose
{
public:
  halp_meta(name, "Viewer From Pose")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "viewer_from_pose")
  halp_meta(authors, "ossia team")
  halp_meta(
      description,
      "A tracked head position (Pose Detector, OSC, OpenTrackIO...) to the viewer point "
      "of the immersive processes: tracker units scaled to metres, moved into the room "
      "frame by a 4x4 transform (column major) and an offset (e.g. from the tracked point "
      "to the eyes), then smoothed by a 1 euro filter (Casiez, Roussel & Vogel 2012, "
      "doi:10.1145/2207676.2208639): little jitter at rest, little lag when moving. "
      "Raise Beta until fast moves stop lagging, lower Min cutoff until rest stops "
      "jittering.")
  halp_meta(uuid, "2b7e4d91-5c3a-4f86-a0e2-d18f6c9b3e57")

  struct ins
  {
    halp::val_port<"Position", halp::xyz_type<float>> position;
    halp::val_port<"Room from tracker", std::array<float, 16>> transform{
        .value = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    halp::hslider_f32<"Scale", halp::range{0.0001, 100., 1.}> scale;
    halp::xyz_spinboxes_f32<"Offset", halp::range{-10., 10., 0.}> offset;
    halp::hslider_f32<"Min cutoff (Hz)", halp::range{0.01, 30., 1.}> min_cutoff;
    halp::hslider_f32<"Beta", halp::range{0., 10., 0.5}> beta;
    halp::hslider_f32<"Derivative cutoff (Hz)", halp::range{0.01, 30., 1.}> d_cutoff;
    halp::toggle<"Bypass filter"> bypass;
    halp::impulse_button<"Reset"> reset;
  } inputs;

  struct
  {
    halp::val_port<"Viewer", halp::xyz_type<float>> viewer;
    halp::val_port<"Raw", halp::xyz_type<float>> raw;
  } outputs;

  std::array<OneEuroFilter, 3> filters;
  std::optional<double> clockOverride;
  std::array<float, 3> last{1e30f, 1e30f, 1e30f};

  double now() const
  {
    if(clockOverride)
      return *clockOverride;
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
  }

  void operator()()
  {
    if(inputs.reset)
      for(auto& f : filters)
        f.reset();
    const auto& m = inputs.transform.value;
    const float s = inputs.scale.value;
    const float p[3]{
        inputs.position.value.x * s, inputs.position.value.y * s,
        inputs.position.value.z * s};
    float w[3];
    for(int r = 0; r < 3; r++)
      w[r] = m[r] * p[0] + m[4 + r] * p[1] + m[8 + r] * p[2] + m[12 + r];
    w[0] += inputs.offset.value.x;
    w[1] += inputs.offset.value.y;
    w[2] += inputs.offset.value.z;
    outputs.raw.value = {w[0], w[1], w[2]};

    const double t = now();
    const bool fresh = w[0] != last[0] || w[1] != last[1] || w[2] != last[2];
    if(inputs.bypass.value)
    {
      outputs.viewer.value = outputs.raw.value;
      return;
    }
    if(!fresh && filters[0].primed)
      return;
    last = {w[0], w[1], w[2]};
    float out[3];
    for(int c = 0; c < 3; c++)
    {
      auto& f = filters[c];
      f.minCutoff = inputs.min_cutoff.value;
      f.beta = inputs.beta.value;
      f.dCutoff = inputs.d_cutoff.value;
      out[c] = float(f(w[c], t));
    }
    outputs.viewer.value = {out[0], out[1], out[2]};
  }
};
}
