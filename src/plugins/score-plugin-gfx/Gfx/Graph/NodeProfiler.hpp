#pragma once
#include <score_plugin_gfx_export.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

class QRhi;
class QRhiCommandBuffer;

namespace score::gfx
{
struct Node;

struct NodeTiming
{
  int32_t node{-1};
  //! typeid name of the node, to label the nodes no process owns.
  const char* type{};
  int64_t cpu_ns{0};
  //! -1 when the backend gave no GPU time for this frame.
  int64_t gpu_ns{-1};
  //! Bytes of the render targets the render list owns for the node's inputs.
  int64_t target_bytes{0};
};

struct FrameTiming
{
  //! Which output rendered the frame.
  const void* list{};
  int64_t frame{};
  //! steady_clock time the frame started, for the frame rate.
  int64_t time_ns{};
  //! Node times measured outside a render list's frame (an output's own
  //! passes): no frame totals, the nodes only.
  bool nodesOnly{};
  int64_t cpu_ns{};
  int64_t gpu_ns{-1};
  std::vector<NodeTiming> nodes;
};

/**
 * @brief Per-node render timings of one GfxContext's outputs.
 *
 * The render lists submit one FrameTiming per frame while it is enabled; with
 * GPU timestamps a frame arrives a few frames after it was rendered. The UI
 * thread takes them. Thread-safe.
 */
class SCORE_PLUGIN_GFX_EXPORT NodeProfiler
{
public:
  bool enabled() const noexcept { return m_enabled.load(std::memory_order_relaxed); }
  void setEnabled(bool b) noexcept;

  void submit(FrameTiming&& f);
  std::vector<FrameTiming> take();

private:
  std::atomic_bool m_enabled{};
  std::mutex m_mutex;
  std::vector<FrameTiming> m_frames;
};

/**
 * @brief Records one render list's per-node timings.
 *
 * CPU time is what the caller measures around each renderer's update and
 * command recording. GPU time comes from timestamps written between passes:
 * each stamp closes the span since the previous one and gives it to the
 * nodes passed, split evenly when several nodes draw into the same pass.
 * Vulkan, D3D12 and desktop OpenGL have GPU time; other backends only CPU time.
 */
class SCORE_PLUGIN_GFX_EXPORT RenderListProfiler
{
public:
  RenderListProfiler(QRhi& rhi, const void* list);
  ~RenderListProfiler();
  RenderListProfiler(const RenderListProfiler&) = delete;
  RenderListProfiler& operator=(const RenderListProfiler&) = delete;

  bool hasGpuTime() const noexcept;

  void beginFrame(QRhiCommandBuffer& cb, NodeProfiler& out, int64_t frame);
  void addCpu(const Node& node, int64_t ns);
  void setTargetBytes(const Node& node, int64_t bytes);
  void stamp(QRhiCommandBuffer& cb, std::span<const Node* const> owners = {});
  void stamp(QRhiCommandBuffer& cb, const Node& owner);
  void endFrame(NodeProfiler& out, int64_t cpu_ns);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
}
