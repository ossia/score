// score_scene_common.glsl — constants and helpers shared by every scene
// preset that reads a Scene Preprocessor output (classic_pbr, pbr_fast,
// blinn_phong, unlit and their _skinned / _cube variants).
//
// The structs themselves cannot live here: the ISF parser emits them from
// the header's TYPES block, before the shader body, and a buffer declaration
// (`layout(...) buffer`) can only come from INPUTS / AUXILIARY. Copy this
// TYPES block verbatim into every preset header so the layouts stay
// byte-identical to the C++ (Gfx/Graph/SceneGPUState.hpp and
// ScenePreprocessorNode.cpp):
//
//   "TYPES": [
//     { "NAME": "Light", "LAYOUT": [                          // RawLightData, 64 B
//         { "NAME": "color",           "TYPE": "vec4" },      // rgb, w = intensity
//         { "NAME": "local_direction", "TYPE": "vec4" },      // xyz, w = type 0 dir / 1 point / 2 spot
//         { "NAME": "range_cone",      "TYPE": "vec4" },      // range, inner cos, outer cos, depth bias
//         { "NAME": "shadow_enabled",  "TYPE": "uint" },
//         { "NAME": "decay_mode",      "TYPE": "uint" },
//         { "NAME": "transform_slot",  "TYPE": "uint" },      // 0xFFFFFFFF = procedural
//         { "NAME": "normal_bias",     "TYPE": "float" } ] },
//     { "NAME": "Material", "LAYOUT": [                       // MaterialGPU, 80 B
//         { "NAME": "baseColor",                       "TYPE": "vec4" },
//         { "NAME": "metallicRoughnessOcclusionUnlit", "TYPE": "vec4" },
//         { "NAME": "emissive_strength",               "TYPE": "vec4" },
//         { "NAME": "textureRefs",                     "TYPE": "uvec4" },  // bc, mr, normal, emissive
//         { "NAME": "feature_mask",                    "TYPE": "uint" },
//         { "NAME": "hit_group_id",                    "TYPE": "uint" },
//         { "NAME": "occlusion_textureRef",            "TYPE": "uint" },
//         { "NAME": "alpha_cutoff",                    "TYPE": "float" } ] },
//     { "NAME": "MaterialExt", "LAYOUT": [                    // MaterialExtensionsGPU, 272 B
//         { "NAME": "coat",                    "TYPE": "vec4" },  // weight, roughness, ior, darkening
//         { "NAME": "coat_anisotropy",         "TYPE": "vec4" },
//         { "NAME": "fuzz_color",              "TYPE": "vec4" },  // sheen rgb, w = sheen roughness
//         { "NAME": "transmission",            "TYPE": "vec4" },  // weight, depth, dispersion, abbe
//         { "NAME": "transmission_color",      "TYPE": "vec4" },
//         { "NAME": "transmission_scatter",    "TYPE": "vec4" },
//         { "NAME": "specular_weight_color",   "TYPE": "vec4" },  // weight, rgb colour
//         { "NAME": "specular_ior_anisotropy", "TYPE": "vec4" },  // ior, aniso strength, rot cos, rot sin
//         { "NAME": "thin_film",               "TYPE": "vec4" },
//         { "NAME": "diffuse_transmission",    "TYPE": "vec4" },
//         { "NAME": "subsurface_weight_color", "TYPE": "vec4" },
//         { "NAME": "subsurface_radius_scale", "TYPE": "vec4" },
//         { "NAME": "flags",                   "TYPE": "vec4" },
//         { "NAME": "textureRefs0",            "TYPE": "uvec4" },
//         { "NAME": "textureRefs1",            "TYPE": "uvec4" },
//         { "NAME": "textureRefs2",            "TYPE": "uvec4" },
//         { "NAME": "textureRefs3",            "TYPE": "uvec4" } ] },
//     { "NAME": "PerDraw", "LAYOUT": [                        // PerDrawGPU, 144 B
//         { "NAME": "model",           "TYPE": "mat4" },
//         { "NAME": "normal",          "TYPE": "mat4" },      // normal[3].xyz = instance flags
//         { "NAME": "material_index",  "TYPE": "uint" },
//         { "NAME": "tag_hash",        "TYPE": "uint" },
//         { "NAME": "transform_slot",  "TYPE": "uint" },
//         { "NAME": "skeleton_offset", "TYPE": "uint" } ] },
//     { "NAME": "MatUVXform", "LAYOUT": [                     // KHR_texture_transform, 7 x vec4
//         { "NAME": "bc_offset_scale",     "TYPE": "vec4" },
//         { "NAME": "mr_offset_scale",     "TYPE": "vec4" },
//         { "NAME": "normal_offset_scale", "TYPE": "vec4" },
//         { "NAME": "em_offset_scale",     "TYPE": "vec4" },
//         { "NAME": "occ_offset_scale",    "TYPE": "vec4" },
//         { "NAME": "rotations0",          "TYPE": "vec4" },  // bc, mr, normal, emissive
//         { "NAME": "rotations1",          "TYPE": "vec4" } ] },  // occlusion, normal scale
//     { "NAME": "DrawIndexedCmd", "LAYOUT": [
//         { "NAME": "indexCount",   "TYPE": "uint" },
//         { "NAME": "instanceCount","TYPE": "uint" },
//         { "NAME": "firstIndex",   "TYPE": "uint" },
//         { "NAME": "baseVertex",   "TYPE": "int"  },
//         { "NAME": "baseInstance", "TYPE": "uint" } ] }
//   ]
//
// The env UBO is EnvParamsUBO: ambient (rgb, intensity), fog_color_density
// (rgb, density), fog_range (start, end, mode, enabled), exposure_gamma.
// shadow_cascades is ShadowCascadesUBO: light_view_proj mat4[8],
// cascade_split_distances vec4 + cascade_split_distances_hi vec4,
// cascade_count uint + 3 pad uints (560 B).

#ifndef SCORE_SCENE_COMMON_GLSL
#define SCORE_SCENE_COMMON_GLSL

// MaterialGPU::feature_mask bits (SceneGPUState.hpp, namespace material_feature).
const uint SCORE_FEAT_HAS_BASE_COLOR_TEX   = 1u << 0u;
const uint SCORE_FEAT_HAS_METAL_ROUGH_TEX  = 1u << 1u;
const uint SCORE_FEAT_HAS_NORMAL_TEX       = 1u << 2u;
const uint SCORE_FEAT_HAS_EMISSIVE_TEX     = 1u << 3u;
const uint SCORE_FEAT_UNLIT                = 1u << 4u;
const uint SCORE_FEAT_ALPHA_NON_OPAQUE     = 1u << 5u;
const uint SCORE_FEAT_HAS_CLEARCOAT        = 1u << 6u;
const uint SCORE_FEAT_HAS_SHEEN            = 1u << 7u;
const uint SCORE_FEAT_HAS_TRANSMISSION     = 1u << 8u;
const uint SCORE_FEAT_HAS_VOLUME           = 1u << 9u;
const uint SCORE_FEAT_HAS_SPECULAR         = 1u << 10u;
const uint SCORE_FEAT_HAS_IRIDESCENCE      = 1u << 11u;
const uint SCORE_FEAT_HAS_ANISOTROPY       = 1u << 12u;
const uint SCORE_FEAT_HAS_DIFFUSE_TRANSM   = 1u << 13u;
const uint SCORE_FEAT_HAS_SUBSURFACE       = 1u << 14u;
const uint SCORE_FEAT_THIN_WALLED          = 1u << 15u;
const uint SCORE_FEAT_ALPHA_MASK           = 1u << 16u;
const uint SCORE_FEAT_ALPHA_BLEND          = 1u << 17u;
const uint SCORE_FEAT_DOUBLE_SIDED         = 1u << 18u;
const uint SCORE_FEAT_HAS_SEPARATE_OCC     = 1u << 19u;
// Two bits per channel, 0 = TEXCOORD_0, 1 = TEXCOORD_1.
const uint SCORE_FEAT_TCSET_BC_SHIFT       = 20u;
const uint SCORE_FEAT_TCSET_MR_SHIFT       = 22u;
const uint SCORE_FEAT_TCSET_NRM_SHIFT      = 24u;
const uint SCORE_FEAT_TCSET_EM_SHIFT       = 26u;
const uint SCORE_FEAT_TCSET_OCC_SHIFT      = 28u;
const uint SCORE_FEAT_SHADOW_CASTER_OFF    = 1u << 30u;
const uint SCORE_FEAT_REFLECTION_CASTER_OFF = 1u << 31u;

bool score_feat(uint feature_mask, uint bit)
{
  return (feature_mask & bit) != 0u;
}

// Packed texture reference: source:2 | bucket:7 | layer:23.
// source 1 = static pool (materialArray<bucket>, layer), 2 = dynamic slot
// (materialDyn<layer>). 0xFFFFFFFF = no texture.
const uint SCORE_TEXREF_NONE = 0xFFFFFFFFu;

bool score_texref_valid(uint ref)
{
  uint src = (ref >> 30u) & 3u;
  return ref != SCORE_TEXREF_NONE && (src == 1u || src == 2u);
}
uint score_texref_source(uint ref) { return (ref >> 30u) & 3u; }
uint score_texref_bucket(uint ref) { return (ref >> 23u) & 0x7Fu; }
uint score_texref_layer(uint ref)  { return ref & 0x007FFFFFu; }

// Light types in Light.local_direction.w.
const float SCORE_LIGHT_DIRECTIONAL = 0.0;
const float SCORE_LIGHT_POINT       = 1.0;
const float SCORE_LIGHT_SPOT        = 2.0;
const uint  SCORE_LIGHT_PROCEDURAL_SLOT = 0xFFFFFFFFu;

float score_max3(vec3 v) { return max(v.x, max(v.y, v.z)); }

// Environment exposure. 0 means "unset" (no Environment node wired), not "black".
float score_env_exposure_value(vec4 exposure_gamma)
{
  return exposure_gamma.x > 0.0 ? exposure_gamma.x : 1.0;
}

#endif
