#pragma once
#include <Threedim/ModelDisplay/ModelDisplayNode.hpp>

#include <optional>

namespace Gfx::GeometryViewer
{
/**
 * Model Display driven by an orbiting camera and viewing modes. Its ports
 * are those of the Geometry viewer process: texture, geometry, mode, yaw,
 * pitch, zoom, centre, field of view, auto-rotate, auto-fit, background,
 * plane, axes.
 */
struct GeometryViewerNode final : score::gfx::ModelDisplayNode
{
  void process(score::gfx::Message&& msg) override;
  void processControl(int32_t port, const ossia::value& v) override;
  void process(int32_t port, const ossia::transform3d& v) override;
  bool fitBounds(const ossia::aabb& box) override;

private:
  //! Frames m_box where the model matrix puts it. Returns whether that moved
  //! the camera.
  bool fit();
  //! Places the camera and picks the shading and guides the controls ask for.
  void updateCamera();

  int m_mode{};
  float m_yaw{35.f}, m_pitch{25.f}, m_zoom{1.f}, m_fov{60.f};
  ossia::vec3f m_center{};
  bool m_spin{};
  float m_spinAngle{};
  bool m_autofit{true};
  int m_background{5};
  bool m_plane{true}, m_axes{true};

  //! The geometry last measured, in its own space, then in world space
  std::optional<ossia::aabb> m_box;
  bool m_fitted{};
  ossia::vec3f m_fitCenter{};
  float m_fitRadius{1.f};
  float m_fitFloor{};
};
}
