#pragma once
#include <Threedim/TinyObj.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>

#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>

#include <cstdint>
#include <memory>

class QRhiResourceUpdateBatch;

namespace score::gfx
{
class RenderList;
struct Edge;
}

namespace Threedim
{

// Unified light producer. One node with a mode combobox covers every
// punctual / area light type ossia::light_component defines —
// directional, point, spot, rect, disk, sphere, cylinder, dome —
// mirroring UsdLux's RectLight/DiskLight/SphereLight and glTF
// KHR_lights_punctual.
//
// Emits an ossia::scene_spec containing one scene_node with:
//   - child[0] = scene_transform (position + rotation, no scale)
//   - child[1] = light_component_ptr
// ScenePreprocessor packs it into the scene-wide `scene_lights` SSBO via
// packLight(). Current consumer shaders (`classic_pbr_*.frag`) only
// sample the common fields (position/direction/color/intensity/range +
// spot cone angles) — area-light shapes pass through correctly but
// are rendered as point-light approximations until shaders add the
// Rect/Disk/Sphere sampling math. That's a shader-side follow-up.
class Light
{
public:
  halp_meta(name, "Light")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "light")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/light.html")
  halp_meta(uuid, "9f3c1a5e-4b7d-4e2a-8c5f-1d6e0b9a3c7f")

  enum Mode
  {
    Directional,
    Point,
    Spot,
    Rect,
    Disk,
    Sphere,
    Dome
  };

  enum Decay
  {
    DecayNone,
    DecayLinear,
    DecayQuadratic,  // physically correct
    DecayCubic
  };

  struct ins
  {
    // Port-driven rebuild: each control's update() callback triggers
    // Light::rebuild() on user change. operator()() just republishes.
    struct : halp::combobox_t<"Mode", Mode>
    {
      struct range
      {
        std::string_view values[7]{
            "Directional", "Point", "Spot",
            "Rect", "Disk", "Sphere", "Dome"};
        int init{0};
      };
      void update(Light& n) { n.rebuild(); }
    } mode;

    // Common — always applies
    struct : halp::color_chooser<"Color">
    { void update(Light& n) { n.rebuild(); } } color;
    struct : halp::hslider_f32<"Intensity", halp::range{0., 100., 1.}>
    { void update(Light& n) { n.rebuild(); } } intensity;
    // range=0 → infinite falloff (directional / dome ignore this field)
    struct : halp::hslider_f32<"Range", halp::range{0., 1000., 0.}>
    { void update(Light& n) { n.rebuild(); } } range;

    struct : halp::combobox_t<"Falloff", Decay>
    {
      struct range
      {
        std::string_view values[4]{
            "None", "Linear", "Quadratic (physical)", "Cubic"};
        int init{2};
      };
      void update(Light& n) { n.rebuild(); }
    } decay;

    // Spot cone (radians via hsliders taking degrees; converted in cpp)
    struct : halp::hslider_f32<"Inner cone °", halp::range{0., 90., 0.}>
    { void update(Light& n) { n.rebuild(); } } inner_cone;
    struct : halp::hslider_f32<"Outer cone °", halp::range{0., 90., 45.}>
    { void update(Light& n) { n.rebuild(); } } outer_cone;

    // Area shapes
    struct : halp::hslider_f32<"Width",  halp::range{0.01, 100., 1.}>
    { void update(Light& n) { n.rebuild(); } } width;
    struct : halp::hslider_f32<"Height", halp::range{0.01, 100., 1.}>
    { void update(Light& n) { n.rebuild(); } } height;
    struct : halp::hslider_f32<"Radius", halp::range{0.01, 100., 0.5}>
    { void update(Light& n) { n.rebuild(); } } radius;

    // Shadow settings
    struct : halp::toggle<"Cast shadow">
    { void update(Light& n) { n.rebuild(); } } cast_shadow;
    struct : halp::hslider_f32<"Shadow bias", halp::range{0., 0.1, 0.001}>
    { void update(Light& n) { n.rebuild(); } } shadow_bias;
    struct : halp::hslider_f32<"Shadow normal bias", halp::range{0., 0.1, 0.01}>
    { void update(Light& n) { n.rebuild(); } } shadow_normal_bias;

    // Transform: position for positional lights, rotation encodes the
    // direction used by Directional / Spot (local -Z mapped to the
    // light direction, glTF / Vulkan convention).
    struct : PositionControl
    { void update(Light& n) { n.rebuild(); } } position;
    struct : RotationControl
    { void update(Light& n) { n.rebuild(); } } rotation;
  } inputs;

  struct outs
  {
    struct
    {
      halp_meta(name, "Scene");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_out;
  } outputs;

  // Built once from control values whenever a port's update() fires.
  // operator()() just republishes m_state.
  void rebuild();
  void operator()();

  // Render-thread hooks. init claims one RawLight slot; update packs
  // color / intensity / type / local-direction / range / cone angles /
  // decay / shadow into a RawLightData and uploads; release returns
  // the slot. Final world-direction composition happens inside the
  // preprocessor (parent-chain world matrix), so this slot carries
  // only the node-local fields.
  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);

  std::shared_ptr<ossia::scene_state> m_state;
  int64_t m_version{0};
  uint8_t m_pending_dirty{ossia::scene_port::dirty_lights};
  // Stable id for the single light_component this node emits. Minted
  // lazily on first rebuild() and reused across all subsequent rebuilds
  // so downstream caches (preprocessor fingerprint, SER coherence key)
  // stay keyed on identity, not pointer.
  uint64_t m_light_stable_id{};
  uint64_t m_xform_stable_id{};

  score::gfx::GpuResourceRegistry::Slot raw_light_slot;
  score::gfx::GpuResourceRegistry::Slot raw_transform_slot;

  // Ossia-facing snapshots. Written once in init() on the render
  // thread; copied onto each emitted light_component / scene_transform
  // raw_slot in operator()() on the execution thread.
  ossia::gpu_slot_ref m_light_ref{};
  ossia::gpu_slot_ref m_xform_ref{};
};

}
