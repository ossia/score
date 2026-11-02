#pragma once
#include <Process/Monitoring/Monitor.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Gfx::GeometryViewer
{
//! What a scene holds, as the viewer counts it
struct SceneCounts
{
  uint32_t nodes{};
  uint32_t meshes{};
  uint32_t primitives{};
  uint64_t vertices{};
  uint32_t materials{};
  uint32_t lights{};
  uint32_t cameras{};
  uint32_t instances{};

  bool operator==(const SceneCounts&) const noexcept = default;
};

//! One object of the scene, in the order of a depth-first walk
struct SceneRow
{
  enum class Kind : uint8_t
  {
    Node,
    Mesh,
    Material,
    Light,
    Camera,
    Skeleton,
    Animation,
    Instance,
    Data,
    Splat,
    Voxels,
    PointCloud,
    Volume,
    PrimitiveCloud
  };

  //! Index of the row it is under, -1 for a root
  int32_t parent{-1};
  Kind kind{};
  std::string name;
  std::string details;
  //! Where it is in the space of the scene; empty when it occupies none
  ossia::aabb bounds{{1.f, 1.f, 1.f}, {-1.f, -1.f, -1.f}};
};

struct SceneSummary
{
  //! Changes with every scene summarised: identifies the rows a selection
  //! refers to
  uint32_t serial{};
  SceneCounts counts;
  std::vector<SceneRow> rows;
};

/**
 * What a Geometry viewer shares with its views: the summary of the scene it
 * draws, written by its renderer when the scene changes, and the object the
 * inspector selected, which the renderer outlines.
 */
struct SceneMonitor final : Process::Monitor
{
  Process::LatestValue<std::shared_ptr<const SceneSummary>> summary;

  //! The serial of a summary in the high 32 bits, a row of it in the low
  //! ones; -1 for none
  std::atomic<int64_t> selected{-1};

  static int64_t selection(uint32_t serial, int32_t row) noexcept
  {
    return row < 0 ? -1 : int64_t((uint64_t(serial) << 32) | uint32_t(row));
  }

  //! GUI thread: the newest summary, kept for every view that asks
  const std::shared_ptr<const SceneSummary>& latest()
  {
    summary.consume(m_latest);
    return m_latest;
  }

private:
  std::shared_ptr<const SceneSummary> m_latest;
};
}
