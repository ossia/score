#pragma once
#include <ossia/dataflow/geometry_port.hpp>

#include <Threedim/GeometryViewer/SceneMonitor.hpp>

#include <QMatrix4x4>

#include <vector>

namespace Gfx::GeometryViewer
{
//! A light or a camera of the scene, where the viewer draws its gizmo
struct SceneGizmo
{
  enum Kind : uint8_t
  {
    Light,
    Camera
  } kind{};
  ossia::light_type light{};
  //! Straight RGB
  float color[3]{1.f, 1.f, 1.f};
  //! Vertical field of view in radians, width over height; 0 for orthographic
  float yfov{}, aspect{1.f};
  QMatrix4x4 world;
  //! Its row in the summary
  int32_t row{-1};
};

//! A primitive to draw where its transforms put it. Points into the scene,
//! valid while it is held.
struct SceneDrawItem
{
  //! Either a primitive, or a geometry of a mesh's legacy geometry
  const ossia::mesh_primitive* primitive{};
  const ossia::geometry_spec* legacy{};
  std::size_t legacy_index{};
  QMatrix4x4 world;
  ossia::material_component_ptr material;
};

//! What the viewer shows of a scene
struct SceneDescription
{
  //! The most draws a scene gets, instances included
  static constexpr std::size_t max_draws = 4096;

  SceneSummary summary;
  std::vector<SceneDrawItem> draws;
  //! Instances left out past max_draws, or whose transforms are not in memory
  uint64_t skipped_instances{};
  //! The union of the bounds of what is drawn, in the space of the scene
  ossia::aabb bounds{{1.f, 1.f, 1.f}, {-1.f, -1.f, -1.f}};
  std::vector<SceneGizmo> gizmos;

  void clear()
  {
    summary.counts = {};
    summary.rows.clear();
    draws.clear();
    skipped_instances = 0;
    bounds = {{1.f, 1.f, 1.f}, {-1.f, -1.f, -1.f}};
    gizmos.clear();
  }
};

//! Walks the scene as the renderers flatten it: transforms apply to the
//! siblings that follow them, inactive nodes are left out.
void describeScene(const ossia::scene_spec& scene, SceneDescription& out);

//! The box around a box once transformed; empty stays empty
ossia::aabb transformBounds(const ossia::aabb& box, const QMatrix4x4& m) noexcept;

void unite(ossia::aabb& into, const ossia::aabb& box) noexcept;

//! The transform of instance `i`, when its transforms are held in memory
bool instanceMatrix(
    const ossia::instance_component& inst, uint32_t i, QMatrix4x4& out) noexcept;

//! How many instances have their transform in memory
uint32_t instancesInMemory(const ossia::instance_component& inst) noexcept;

//! A name to show for a material
std::string materialName(
    const ossia::material_component& m, const ossia::scene_state* state);
}
