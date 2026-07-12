#pragma once

#include <vector>

class QRhiBuffer;

namespace score::gfx
{

// Draw-time bindings the renderer must merge into its vertex-input
// array to satisfy "REQUIRED: false" VERTEX_INPUTS whose upstream
// geometry did not provide a matching attribute.
//
// Emitted by the fallback-aware remapPipelineVertexInputs overload and
// consumed by RenderedRawRasterPipelineNode at draw time. Each Slot has
// a `binding_index` — the slot in the pipeline's vertex-input binding
// array that was appended during pipeline build — and a QRhiBuffer* the
// runtime binds at that index when issuing the draw.
//
// The plan is safe to hold across frames: the buffer handles come from
// the VertexFallbackPool which lives alongside the RenderList.
//
// This struct lives in its own header so consumers (CustomMesh, the
// renderer) can depend on it without pulling the full Utils.hpp /
// VertexFallbackPool.hpp graph in via Mesh.hpp.
struct FallbackBindingPlan
{
  struct Slot
  {
    int binding_index{};
    QRhiBuffer* buffer{};
  };
  std::vector<Slot> slots;

  // Which of the upstream geometry's vertex bindings the pipeline
  // actually consumes, in pipeline binding order: `mesh_bindings[k]` is
  // the index into `geometry.bindings` / `geometry.input` that pipeline
  // binding k was built from.
  //
  // A geometry publishes every stream it has; a shader reads the handful
  // it declares. Without this the draw binds all of them, and the count
  // is what the backend sees: Qt's D3D11 command buffer carries at most
  // MAX_VERTEX_BUFFER_BINDING_COUNT == 8 (qrhid3d11_p.h) and silently
  // drops the rest. Compacting to the consumed set keeps the draw within
  // reach of the narrowest backend and skips buffer binds nothing reads.
  //
  // Only meaningful when `compacted` is true; the pipeline builders that
  // do not compute a plan leave it false, and the draw path then binds
  // the geometry's inputs one-to-one as before.
  std::vector<int> mesh_bindings;
  bool compacted{false};

  bool empty() const noexcept { return slots.empty() && !compacted; }
  void clear() noexcept
  {
    slots.clear();
    mesh_bindings.clear();
    compacted = false;
  }
};

} // namespace score::gfx
