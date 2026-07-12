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

  bool empty() const noexcept { return slots.empty(); }
  void clear() noexcept { slots.clear(); }
};

} // namespace score::gfx
