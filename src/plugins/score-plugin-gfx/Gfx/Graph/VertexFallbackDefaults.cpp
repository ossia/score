#include <Gfx/Graph/VertexFallbackDefaults.hpp>

#include <ossia/detail/hash.hpp>

#include <cstring>

namespace score::gfx
{
namespace
{

// Small helper: how many float components does a GLSL TYPE declare?
// Returns 0 for unsupported types (mat4, integer types) — v1 accepts
// only scalar float / vec2 / vec3 / vec4 inputs for the fallback path.
// This is strict on purpose: the PerInstance step_rate=1 broadcast
// semantics we ship don't generalise cleanly to integer IDs or mat4
// (location-bump issue).
int float_components_of(std::string_view decl_type) noexcept
{
  if(decl_type == "float") return 1;
  if(decl_type == "vec2")  return 2;
  if(decl_type == "vec3")  return 3;
  if(decl_type == "vec4")  return 4;
  return 0;
}

// Map component count to the matching ossia geometry attribute format.
// Only float formats are emitted in v1.
int format_for_components(int n) noexcept
{
  using F = ossia::geometry::attribute;
  switch(n)
  {
    case 1: return F::float1;
    case 2: return F::float2;
    case 3: return F::float3;
    case 4: return F::float4;
    default: return F::float4;
  }
}

// Pack `n` floats into the spec's byte buffer starting at offset 0.
// `src` holds the source numbers; values past src.size() are zero-padded.
void pack_floats(VertexFallbackSpec& spec, int n,
                 std::initializer_list<float> src) noexcept
{
  float tmp[4] = {0.f, 0.f, 0.f, 0.f};
  int i = 0;
  for(auto v : src) { if(i < 4) tmp[i++] = v; }
  std::memcpy(spec.bytes.data(), tmp, (size_t)n * sizeof(float));
  spec.stride_bytes = (uint32_t)(n * sizeof(float));
  spec.format = format_for_components(n);
}

// Canonical whitelist of neutrals. Returns true if `semantic` is
// whitelisted and the spec has been filled; returns false for
// semantics that require an explicit user DEFAULT.
//
// Keep this in sync with the table in
// docs/reference-manual/processes/library/render-pipeline.md.
bool fill_whitelist(VertexFallbackSpec& spec,
                    ossia::attribute_semantic sem, int n) noexcept
{
  using S = ossia::attribute_semantic;
  switch(sem)
  {
    // Core geometry
    case S::position:         pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;
    case S::normal:           pack_floats(spec, n, {0.f, 0.f, 1.f, 0.f}); return true;
    case S::tangent:          pack_floats(spec, n, {1.f, 0.f, 0.f, 1.f}); return true;
    case S::bitangent:        pack_floats(spec, n, {0.f, 1.f, 0.f, 0.f}); return true;

    // UVs
    case S::texcoord0: case S::texcoord1: case S::texcoord2: case S::texcoord3:
    case S::texcoord4: case S::texcoord5: case S::texcoord6: case S::texcoord7:
      pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;

    // Vertex colors — multiplicative identity is white.
    case S::color0: case S::color1: case S::color2: case S::color3:
      pack_floats(spec, n, {1.f, 1.f, 1.f, 1.f}); return true;

    // Per-instance broadcast colors — same multiplicative identity as
    // their per-vertex counterparts. Drives the unified-MDI shader's
    // base × inst_color modulation: when no per-instance binding is
    // present (Sponza, plain glTF), every fragment reads white and the
    // effective scaling collapses to per-vertex × material only.
    case S::instance_color0: case S::instance_color1:
    case S::instance_color2: case S::instance_color3:
      pack_floats(spec, n, {1.f, 1.f, 1.f, 1.f}); return true;

    // Per-instance custom — application-specific user data. Zero is the
    // benign default for "ignore me unless wired".
    case S::instance_custom0: case S::instance_custom1:
    case S::instance_custom2: case S::instance_custom3:
      pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;

    // instance_draw_id intentionally omitted — uint-typed VERTEX_INPUTs
    // aren't supported by the float-only v1 fallback path. Unified-MDI
    // shaders that read it must set REQUIRED: true (and the
    // ScenePreprocessor publishes the per-instance draw_id buffer).

    // Transform / instancing. The enum at rotation..translation
    // (values 600..607) is now collision-free with the morph deltas
    // (500..504), so every transform semantic has an unambiguous
    // neutral. transform_matrix (mat4) is still intentionally absent:
    // mat4 VERTEX_INPUTS need distinct per-column vertex-input
    // bindings which the v1 fallback path (single PerInstance buffer,
    // single float{1..4} format) cannot express. Users can declare
    // four vec4 columns and reassemble in GLSL, or keep
    // transform_matrix REQUIRED: true.
    case S::rotation:         pack_floats(spec, n, {0.f, 0.f, 0.f, 1.f}); return true;
    case S::rotation_extra:   pack_floats(spec, n, {0.f, 0.f, 0.f, 1.f}); return true;
    case S::scale:            pack_floats(spec, n, {1.f, 1.f, 1.f, 1.f}); return true;
    case S::uniform_scale:    pack_floats(spec, n, {1.f, 0.f, 0.f, 0.f}); return true;
    case S::up:               pack_floats(spec, n, {0.f, 1.f, 0.f, 0.f}); return true;
    case S::pivot:            pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;
    case S::translation:      pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;

    // Morph deltas — zero delta means "no morph contribution", which is
    // exactly the right neutral for an absent morph target. All five
    // are safe to include now that the collisions are gone.
    case S::morph_position:
    case S::morph_normal:
    case S::morph_tangent:
    case S::morph_texcoord:
    case S::morph_color:
      pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;

    // Particle dynamics — at-rest defaults.
    case S::velocity:
    case S::acceleration:
    case S::force:
    case S::angular_velocity:
      pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;
    case S::mass:             pack_floats(spec, n, {1.f, 0.f, 0.f, 0.f}); return true;
    case S::age:              pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;
    case S::lifetime:         pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;
    case S::drag:             pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;

    // Rendering hints
    case S::sprite_size:      pack_floats(spec, n, {1.f, 1.f, 0.f, 0.f}); return true;
    case S::sprite_rotation:  pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;
    case S::sprite_facing:    pack_floats(spec, n, {0.f, 0.f, 1.f, 0.f}); return true;
    case S::width:            pack_floats(spec, n, {1.f, 0.f, 0.f, 0.f}); return true;
    case S::opacity:          pack_floats(spec, n, {1.f, 0.f, 0.f, 0.f}); return true;
    case S::emissive:         pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;
    case S::emissive_strength: pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;

    // Material / PBR
    case S::roughness:        pack_floats(spec, n, {0.5f, 0.f, 0.f, 0.f}); return true;
    case S::metallic:         pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;
    case S::ambient_occlusion: pack_floats(spec, n, {1.f, 0.f, 0.f, 0.f}); return true;
    case S::specular:         pack_floats(spec, n, {0.5f, 0.f, 0.f, 0.f}); return true;
    case S::subsurface:
    case S::clearcoat:
    case S::clearcoat_roughness:
    case S::anisotropy:
    case S::transmission:
    case S::thickness:
      pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;
    case S::anisotropy_direction: pack_floats(spec, n, {1.f, 0.f, 0.f, 0.f}); return true;
    case S::ior:              pack_floats(spec, n, {1.5f, 0.f, 0.f, 0.f}); return true;

    // UI / effect slots
    case S::selection:        pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;
    case S::fx0: case S::fx1: case S::fx2: case S::fx3:
    case S::fx4: case S::fx5: case S::fx6: case S::fx7:
      pack_floats(spec, n, {0.f, 0.f, 0.f, 0.f}); return true;

    // Everything else: NOT whitelisted. Forces the caller to require an
    // explicit DEFAULT (motion-history semantics, skinning indices /
    // weights, integer IDs, volumetric / splat data — cases where a
    // wrong "neutral" is silently wrong).
    default:
      return false;
  }
}

} // namespace

std::optional<VertexFallbackSpec> resolveVertexFallback(
    ossia::attribute_semantic semantic,
    std::string_view decl_type,
    const std::vector<double>& user_default) noexcept
{
  const int n = float_components_of(decl_type);
  if(n <= 0)
    return std::nullopt;   // unsupported type (mat4, integer, sampler, ...)

  VertexFallbackSpec spec{};

  if(!user_default.empty())
  {
    // User DEFAULT wins. Pack at most n floats, zero-pad the rest.
    float tmp[4] = {0.f, 0.f, 0.f, 0.f};
    const int k = (int)std::min<std::size_t>(user_default.size(), (std::size_t)n);
    for(int i = 0; i < k; ++i)
      tmp[i] = (float)user_default[(std::size_t)i];
    std::memcpy(spec.bytes.data(), tmp, (size_t)n * sizeof(float));
    spec.stride_bytes = (uint32_t)(n * sizeof(float));
    spec.format = format_for_components(n);
    return spec;
  }

  // No user default — look up the whitelist.
  if(fill_whitelist(spec, semantic, n))
    return spec;

  return std::nullopt;
}

uint64_t hashVertexFallback(const VertexFallbackSpec& spec) noexcept
{
  // rapidhash-tiered (ossia::hash_*); same primitive used everywhere
  // else in the gfx pipeline. Mix format + stride into the seed via
  // hash_combine, then fold in the active byte range so two specs
  // with identical bytes but different formats / strides don't alias.
  uint64_t seed = ossia::hash_trivial(spec.format);
  ossia::hash_combine(seed, spec.stride_bytes);
  const uint32_t active
      = std::min<uint32_t>(spec.stride_bytes, (uint32_t)spec.bytes.size());
  ossia::hash_combine(seed, ossia::hash_bytes(spec.bytes.data(), active));
  return seed;
}

} // namespace score::gfx
