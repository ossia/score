#pragma once
#include <QString>

#include <Eigen/Geometry>

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace Threedim
{
//! The surfaces of a room, as triangles in its own frame (metres, +Y up),
//! with a bounding volume hierarchy for casting rays into it.
class RoomGeometry
{
public:
  RoomGeometry();
  ~RoomGeometry();
  RoomGeometry(RoomGeometry&&) noexcept;
  RoomGeometry& operator=(RoomGeometry&&) noexcept;

  //! An axis-aligned box around the origin: the distances from the origin to
  //! the front (-Z), back (+Z), left (-X), right (+X) walls, floor (-Y) and
  //! ceiling (+Y). A distance of 0 leaves that side open: the sides next to it
  //! go on, and a ray through it meets nothing.
  static RoomGeometry box(
      float front, float back, float left, float right, float floor, float ceiling);
  //! A Wavefront OBJ (read with ObjFromString) or a PLY mesh (its faces, in
  //! any element order); empty with `error` set when it cannot be read.
  static RoomGeometry load(const QString& path, QString* error = nullptr);

  bool empty() const noexcept { return m_triangles.empty(); }
  std::size_t triangleCount() const noexcept { return m_triangles.size() / 9; }

  //! Distance along the unit ray to the nearest surface, or a negative value.
  float cast(const std::array<float, 3>& origin, const std::array<float, 3>& dir) const;

  void addTriangle(const std::array<float, 9>& t);
  void build();

private:
  std::vector<float> m_triangles;
  struct Node;
  std::vector<Node> m_nodes;
  std::vector<int> m_order;
};

//! The rotation of Euler degrees (x, y, z) as QQuaternion::fromEulerAngles
//! takes them: about Z, then X, then Y.
inline Eigen::Matrix3d eulerRotation(const std::array<float, 3>& deg)
{
  return (Eigen::AngleAxisd(deg[1] * M_PI / 180., Eigen::Vector3d::UnitY())
          * Eigen::AngleAxisd(deg[0] * M_PI / 180., Eigen::Vector3d::UnitX())
          * Eigen::AngleAxisd(deg[2] * M_PI / 180., Eigen::Vector3d::UnitZ()))
      .toRotationMatrix();
}

//! World points of a direction map (RGBA, unit directions in the camera's
//! frame, w = valid, top row first): each direction is turned into the room's
//! frame by the camera pose (position and Euler degrees, as
//! QQuaternion::fromEulerAngles), cast from the camera's position, and the
//! nearest hit becomes the point (w = 0 where it hits nothing).
struct CastResult
{
  std::vector<float> points;
  double hitFraction{};
};
CastResult castDirections(
    const std::vector<float>& directions, int width, int height, const RoomGeometry& room,
    const std::array<float, 3>& cameraPosition, const std::array<float, 3>& cameraRotation);
}
