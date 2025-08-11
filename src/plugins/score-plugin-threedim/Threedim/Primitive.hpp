#pragma once

#include <Threedim/TinyObj.hpp>
#include <halp/audio.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

namespace Threedim
{
struct Primitive
{
  halp_meta(category, "Visuals/3D/Primitives")
  halp_meta(author, "Jean-Michaël Celerier, vcglib")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/meshes.html#primitive")

  void operator()() { }
  PrimitiveOutputs outputs;
  std::vector<float> complete;
};

// Plane is a special case due to needing a different geometry type
// to disable back-face culling
struct Plane
{
public:
  halp_meta(category, "Visuals/3D/Primitives")
  halp_meta(author, "Jean-Michaël Celerier, vcglib")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/meshes.html#primitive")
  halp_meta(name, "Plane")
  halp_meta(c_name, "3d_plane")
  halp_meta(uuid, "1e923d52-3494-49e8-8698-b001405000da")

  struct
  {
    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;
    struct : halp::spinbox_i32<"H divs.", halp::range{2, 200, 16}>, Update {} hdivs;
    struct : halp::spinbox_i32<"V divs.", halp::range{2, 200, 16}>, Update {} vdivs;
  } inputs;

  struct
  {
    struct
    {
      halp_meta(name, "Geometry");
      halp::position_normals_texcoords_geometry_plane mesh;
      float transform[16]{};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } geometry;
  } outputs;

  void prepare(halp::setup) { update(); }
  void update();
  void operator()() { }

  std::vector<float> complete;
};

struct Cube : Primitive
{
public:
  halp_meta(name, "Cube")
  halp_meta(c_name, "3d_cube")
  halp_meta(uuid, "cf8a328a-1ba6-47f8-929f-2168bdec90b0")

  struct
  {
    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;
  } inputs;

  void prepare(halp::setup) { update(); }
  void update();
};

struct Sphere : Primitive
{
public:
  halp_meta(name, "Sphere")
  halp_meta(c_name, "3d_sphere")
  halp_meta(uuid, "fc0df335-d0e9-4ebf-b438-6ba334741c1a")

  struct
  {
    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;
    struct
        : halp::hslider_i32<"Subdivisions", halp::range{1, 5, 2}>
        , Update
    {
    } subdiv;
  } inputs;

  void prepare(halp::setup) { update(); }
  void update();
};

struct Icosahedron : Primitive
{
  halp_meta(name, "Icosahedron")
  halp_meta(c_name, "3d_ico")
  halp_meta(uuid, "3ea9f69f-1a0e-49c2-ad16-a88e9ca628a7")

  struct
  {
    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;
  } inputs;

  void prepare(halp::setup) { update(); }
  void update();
};

struct Cone : Primitive
{
  halp_meta(name, "Cone")
  halp_meta(c_name, "3d_cone")
  halp_meta(uuid, "8a5718c4-07f0-476b-b720-1c99e5a379a5")

  struct
  {
    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;
    struct
        : halp::hslider_i32<"Subdivisions", halp::range{1, 500, 36}>
        , Update
    {
    } subdiv;
    struct
        : halp::hslider_f32<"R1", halp::range{0, 1000, 1}>
        , Update
    {
    } r1;
    struct
        : halp::hslider_f32<"R2", halp::range{0, 1000, 10}>
        , Update
    {
    } r2;
    struct
        : halp::hslider_f32<"Height", halp::range{0, 1000, 5}>
        , Update
    {
    } h;
  } inputs;

  void prepare(halp::setup) { update(); }
  void update();
};

struct Cylinder : Primitive
{
  halp_meta(name, "Cylinder")
  halp_meta(c_name, "3d_cylinder")
  halp_meta(uuid, "5992830e-80fe-4461-b357-2c9b5c5e48ae")

  struct
  {
    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;
    struct
        : halp::hslider_i32<"Slices", halp::range{1, 64, 16}>
        , Update
    {
    } slices;
    struct
        : halp::hslider_i32<"Stacks", halp::range{1, 64, 16}>
        , Update
    {
    } stacks;
  } inputs;

  void prepare(halp::setup) { update(); }
  void update();
};

struct Torus : Primitive
{
  halp_meta(name, "Torus")
  halp_meta(c_name, "3d_torus")
  halp_meta(uuid, "85c5983c-3f4f-4bfe-b8cf-fccdf6ec5faf")

  struct
  {
    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;
    struct
        : halp::hslider_f32<"R1", halp::range{0, 100, 10}>
        , Update
    {
    } r1;
    struct
        : halp::hslider_f32<"R2", halp::range{0, 100, 1}>
        , Update
    {
    } r2;
    struct
        : halp::hslider_i32<"H Divisions", halp::range{1, 50, 24}>
        , Update
    {
    } hdiv;
    struct
        : halp::hslider_i32<"V Divisions", halp::range{1, 50, 12}>
        , Update
    {
    } vdiv;
  } inputs;

  void prepare(halp::setup) { update(); }
  void update();
};

}
