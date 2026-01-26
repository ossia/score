#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

#include <cstdlib>
#include <cstring>
#include <random>
#include <span>

namespace Threedim
{
struct GeometryPort
{
  halp::dynamic_geometry mesh;
  float transform[16]{
      1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f,
  };
  bool dirty_mesh = false;
  bool dirty_transform = false;
};
struct DeformationControl
{
  enum enum_type
  {
    None,
    Noise,
    Sine
  } value{};

  enum widget
  {
    enumeration,
    list,
    combobox
  };

  struct range
  {
    std::string_view values[3]{"None", "Noise", "Sine"};
    enum_type init = enum_type::Noise;
  };

  operator enum_type&() noexcept { return value; }
  operator const enum_type&() const noexcept { return value; }
  auto& operator=(enum_type t) noexcept
  {
    value = t;
    return *this;
  }
};

struct Noise
{
  halp_meta(name, "Mesh Noise")
  halp_meta(c_name, "mesh_noise")
  halp_meta(uuid, "4f493663-3739-43df-94b5-20a31c4dc8aa")
  halp_meta(category, "Visuals/Meshes/Modifiers")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/meshes.html#noise")
  halp_meta(author, "Jean-Michaël Celerier")

  struct
  {
    struct : GeometryPort
    {
      halp_meta(name, "Geometry");
    } geometry;
    struct : DeformationControl
    {
      halp_meta(name, "Deformation X");
    } dx;
    struct : DeformationControl
    {
      halp_meta(name, "Deformation Y");
    } dy;
    struct : DeformationControl
    {
      halp_meta(name, "Deformation Z");
    } dz;
    halp::hslider_f32<"Intensity X"> ix;
    halp::hslider_f32<"Intensity Y"> iy;
    halp::hslider_f32<"Intensity Z"> iz;

  } inputs;

  struct
  {
    struct : GeometryPort
    {
      halp_meta(name, "Geometry");
    } geometry;
  } outputs;

  Noise();
  ~Noise();

  void operator()();

  int64_t position_in_frames = 0;
};
}
