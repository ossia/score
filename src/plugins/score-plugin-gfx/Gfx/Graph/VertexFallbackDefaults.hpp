#pragma once

#include <ossia/dataflow/geometry_port.hpp>

#include <score_plugin_gfx_export.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace score::gfx
{

// Packed neutral value for an optional VERTEX_INPUT whose upstream
// attribute is absent. The renderer uploads these `stride_bytes` bytes
// into a PerInstance step_rate=1 buffer of exactly one element and binds
// it at the shader input's slot. Stride and format are driven by the
// GLSL TYPE the shader declared — not the semantic's canonical width.
struct VertexFallbackSpec
{
  // Values from the anonymous enum in ossia::geometry::attribute — we
  // store as int to sidestep the "decltype on non-static member"
  // boilerplate; callers cast back at the QRhi boundary the same way
  // RenderedCSFNode.cpp already does.
  int format{};
  uint32_t stride_bytes{};
  // First `stride_bytes` bytes are the payload (native float / int
  // bytes). 64 bytes accommodate mat4 if mat4 VERTEX_INPUTS ever land
  // (they don't today — the parser's location-bump is not mat4-aware).
  std::array<uint8_t, 64> bytes{};
};

// Resolve a fallback for a shader-declared optional VERTEX_INPUT.
//
//   `semantic`       the resolved ossia semantic (from SEMANTIC field if
//                    set, else from NAME via ossia::name_to_semantic).
//                    Pass attribute_semantic::custom for unknown names.
//   `decl_type`      the GLSL TYPE the shader declared, lowercased
//                    ("float", "vec2", "vec3", "vec4"). mat4 / integer
//                    types are unsupported in v1 — returns nullopt.
//   `user_default`   the DEFAULT[] array from the JSON header (may be
//                    empty). When non-empty, overrides the semantic
//                    whitelist: numbers are packed into the payload in
//                    declaration order, then truncated / zero-padded to
//                    fit the declared type width.
//
// Returns `std::nullopt` when neither a user DEFAULT nor a whitelisted
// semantic default applies — the caller is expected to fail the pipeline
// build with a clear error referencing the input name.
SCORE_PLUGIN_GFX_EXPORT std::optional<VertexFallbackSpec> resolveVertexFallback(
    ossia::attribute_semantic semantic,
    std::string_view decl_type,
    const std::vector<double>& user_default) noexcept;

// Stable hash of a fallback spec's byte payload. Used as part of the
// VertexFallbackPool key so two shaders declaring the same semantic and
// TYPE with different DEFAULT arrays don't share a buffer.
SCORE_PLUGIN_GFX_EXPORT uint64_t
hashVertexFallback(const VertexFallbackSpec& spec) noexcept;

}
