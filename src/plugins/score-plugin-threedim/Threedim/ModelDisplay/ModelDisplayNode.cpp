#include "ModelDisplayNode.hpp"
#include <Gfx/Graph/MipGeneration.hpp>

#include <Gfx/Graph/GeometryFilterNodeRenderer.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <ossia/detail/algorithms.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/RhiComputeBarrier.hpp>
#include <boost/algorithm/string.hpp>
#include <ossia/detail/fmt.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/small_vector.hpp>
#include <ossia/gfx/port_index.hpp>
#include <ossia/network/value/value.hpp>
#include <ossia/network/value/value_conversion.hpp>
#include <score/tools/Debug.hpp>
#include <score/tools/SafeCast.hpp>

#include <Threedim/ModelDisplay/ModelSceneDrawer.hpp>

#include <QPainter>

#include <cstring>
#include <optional>

#if defined(near)
#undef near
#undef far
#endif

namespace score::gfx
{

#define model_display_default_uniforms \
  "\
layout(std140, binding = 0) uniform renderer_t { \n\
  mat4 clipSpaceCorrMatrix; \n\
  vec2 renderSize; \n\
} renderer; \n\
 \n\
mat4 clipSpaceCorrMatrix = renderer.clipSpaceCorrMatrix; \n\
// Time-dependent uniforms, only relevant during execution \n\
layout(std140, binding = 1) uniform process_t { \n\
  float TIME; \n\
  float TIMEDELTA; \n\
  float PROGRESS; \n\
  float SAMPLERATE; \n\
 \n\
  int PASSINDEX; \n\
  int FRAMEINDEX; \n\
 \n\
  vec2 RENDERSIZE; \n\
  vec4 DATE; \n\
 \n\
} isf_process_uniforms; \n\
 \n\
float TIME = isf_process_uniforms.TIME; \n\
float TIMEDELTA = isf_process_uniforms.TIMEDELTA; \n\
float PROGRESS = isf_process_uniforms.PROGRESS; \n\
int PASSINDEX = isf_process_uniforms.PASSINDEX; \n\
int FRAMEINDEX = isf_process_uniforms.FRAMEINDEX; \n\
vec2 RENDERSIZE = isf_process_uniforms.RENDERSIZE; \n\
vec4 DATE = isf_process_uniforms.DATE; \n\
\n\
layout(std140, binding = 2) uniform camera_t { \n\
      mat4 matrixModelViewProjection; \n\
      mat4 matrixModelView; \n\
      mat4 matrixModel; \n\
      mat4 matrixView; \n\
      mat4 matrixProjection; \n\
      mat3 matrixNormal; \n\
      float fov; \n\
      float near; \n\
      float far; \n\
} camera; \n\
 \n\
"

const constexpr auto vtx_output_triangle = R"_(
out gl_PerVertex {
vec4 gl_Position;
};
)_";
const constexpr auto vtx_output_point = R"_(
out gl_PerVertex {
vec4 gl_Position;
float gl_PointSize;
};
)_";

const constexpr auto vtx_projection_perspective = R"_(
vec4 v_projected = camera.matrixModelViewProjection * vec4(in_position.xyz, 1.0);
)_";
// Fulldome fisheye projections. All four share the θ/φ derivation and the
// reverse-Z depth and differ only in r_ndc = f(θ). Separate vertex-shader
// snippets rather than a runtime branch, so the GPU runs branch-free code.
//
//   equidistant   r = θ / (FOV/2)           domemaster, uniform angular res
//   equisolid     r = sin(θ/2) / sin(FOV/4) equal-area, photographic fisheye
//   stereographic r = tan(θ/2) / tan(FOV/4) conformal, "little planet"
//   orthographic  r = sin(θ)   / sin(FOV/2) parallel-projection, FOV <= 180
//
// r_ndc > 1 falls outside the NDC unit square and is hardware-clipped, so
// FOV > 180 works for all but orthographic, which cannot exceed it
// geometrically.
const constexpr auto vtx_projection_fulldome_equidistant = R"_(
//
// Fulldome / domemaster (equidistant angular fisheye).
//
//   r_2D  = theta / (fov/2), the radial image distance in NDC units
//   theta = angle from the dome forward axis, view-space +Z here
//   phi   = azimuth around that axis
//
// The .xzy swizzle re-orients world +Z as dome-up and world +Y as
// dome-forward; the view matrix then places the zenith along view-space +Z.
//
// Depth is linear reverse-Z in radial distance: z_gl in [-1,+1] such that
// clipSpaceCorrMatrix yields z_vulkan = 1 at near and 0 at far, matching the
// project-wide convention (depth cleared to 0.0, compare op Greater).
//
vec4 v_projected = vec4(0.0, 0.0, 0.0, 1.0);
{
  vec4 viewspace = camera.matrixModelView * vec4(in_position.xyz, 1.0);
  vec3 d = viewspace.xyz;
  float r = length(d);

  if(r > 1e-6)
  {
    float theta = acos(clamp(-d.z / r, -1.0, 1.0));
    float phi   = (length(d.xy) > 1e-6) ? atan(d.y, d.x) : 0.0;
    float half_fov_rad = max(radians(camera.fov * 0.5), 1e-6);
    float r_ndc = theta / half_fov_rad;

    v_projected.x = r_ndc * cos(phi);
    v_projected.y = r_ndc * sin(phi);
  }

  // Reverse-Z linear depth: z_gl = 1 at r = near (gets remapped to
  // z_vulkan = 1 by clipSpaceCorrMatrix), z_gl = -1 at r = far.
  float t = clamp(
      (r - camera.near) / max(camera.far - camera.near, 1e-6),
      0.0, 1.0);
  v_projected.z = 1.0 - 2.0 * t;
  v_projected.w = 1.0;
}
)_";

// Equisolid-angle (equal-area fisheye). Matches the response of most
// physical fisheye camera lenses (Nikon, Canon). Areas-on-the-sphere map
// to equal areas-on-the-image, so the edge gets less angular resolution
// than the centre.
const constexpr auto vtx_projection_fulldome_equisolid = R"_(
vec4 v_projected = vec4(0.0, 0.0, 0.0, 1.0);
{
  vec4 viewspace = camera.matrixModelView * vec4(in_position.xyz, 1.0);
  vec3 d = viewspace.xyz;
  float r = length(d);

  if(r > 1e-6)
  {
    float theta = acos(clamp(-d.z / r, -1.0, 1.0));
    float phi   = (length(d.xy) > 1e-6) ? atan(d.y, d.x) : 0.0;
    float quarter_fov_rad = max(radians(camera.fov * 0.25), 1e-6);
    float r_ndc = sin(theta * 0.5) / sin(quarter_fov_rad);

    v_projected.x = r_ndc * cos(phi);
    v_projected.y = r_ndc * sin(phi);
  }

  float t = clamp(
      (r - camera.near) / max(camera.far - camera.near, 1e-6),
      0.0, 1.0);
  v_projected.z = 1.0 - 2.0 * t;
  v_projected.w = 1.0;
}
)_";

// Stereographic fisheye. Conformal — local angles / shapes preserved,
// circles on the sphere stay circles in the image. No edge compression of
// shape. Good for VR / architectural preview, less good for uniform
// pixel-per-degree on a dome.
const constexpr auto vtx_projection_fulldome_stereographic = R"_(
vec4 v_projected = vec4(0.0, 0.0, 0.0, 1.0);
{
  vec4 viewspace = camera.matrixModelView * vec4(in_position.xyz, 1.0);
  vec3 d = viewspace.xyz;
  float r = length(d);

  if(r > 1e-6)
  {
    float theta = acos(clamp(-d.z / r, -1.0, 1.0));
    float phi   = (length(d.xy) > 1e-6) ? atan(d.y, d.x) : 0.0;
    float quarter_fov_rad = max(radians(camera.fov * 0.25), 1e-6);
    // tan diverges at θ=π; rely on hardware clipping for θ ≥ FOV/2.
    float r_ndc = tan(theta * 0.5) / tan(quarter_fov_rad);

    v_projected.x = r_ndc * cos(phi);
    v_projected.y = r_ndc * sin(phi);
  }

  float t = clamp(
      (r - camera.near) / max(camera.far - camera.near, 1e-6),
      0.0, 1.0);
  v_projected.z = 1.0 - 2.0 * t;
  v_projected.w = 1.0;
}
)_";

// Orthographic sphere projection. Parallel projection — the image looks
// like a billiard-ball photographed from infinity. FOV must be ≤ 180°;
// beyond that the mapping collapses (sin(θ) decreases past π/2).
const constexpr auto vtx_projection_fulldome_orthographic = R"_(
vec4 v_projected = vec4(0.0, 0.0, 0.0, 1.0);
{
  vec4 viewspace = camera.matrixModelView * vec4(in_position.xyz, 1.0);
  vec3 d = viewspace.xyz;
  float r = length(d);

  if(r > 1e-6)
  {
    float theta = acos(clamp(-d.z / r, -1.0, 1.0));
    float phi   = (length(d.xy) > 1e-6) ? atan(d.y, d.x) : 0.0;
    float half_fov_rad = max(radians(camera.fov * 0.5), 1e-6);
    float r_ndc = sin(theta) / sin(half_fov_rad);

    v_projected.x = r_ndc * cos(phi);
    v_projected.y = r_ndc * sin(phi);
  }

  float t = clamp(
      (r - camera.near) / max(camera.far - camera.near, 1e-6),
      0.0, 1.0);
  v_projected.z = 1.0 - 2.0 * t;
  v_projected.w = 1.0;
}
)_";
const constexpr auto vtx_output_process_triangle = R"_()_";
const constexpr auto vtx_output_process_point = R"_(
  gl_PointSize = 1.0f;
)_";

const constexpr auto model_display_vertex_shader_phong = R"_(#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texcoord;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec3 esVertex;
layout(location = 1) out vec3 esNormal;
layout(location = 2) out vec2 v_texcoord;

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;

%vtx_define_filters%

%vtx_output%

void main()
{
  vec3 in_position = position;
  vec3 in_normal = normal;
  vec2 in_uv = texcoord;
  vec3 in_tangent = vec3(0);
  vec4 in_color = vec4(1);

  %vtx_do_filters%

  esVertex = in_position;
  esNormal = in_normal;
#if !defined(QSHADER_SPIRV)
  v_texcoord = in_uv;
#else
  v_texcoord = vec2(in_uv.x, 1. - in_uv.y);
#endif

  %vtx_do_projection%

  gl_Position = renderer.clipSpaceCorrMatrix * v_projected;
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  // Y convention, as in ImageNode: GL is Y-up with no flip, Vulkan's flip is
  // baked into QRhi's clipSpaceCorrMatrix, and D3D/Metal share Vulkan's
  // framebuffer origin without its NDC sign convention. Flip here so the
  // offscreen texture lands top-row-first on every backend, leaving the screen
  // compositor's SPIRV-only UV flip alone.
  gl_Position.y = -gl_Position.y;
#endif

  %vtx_output_process%
}
)_";

// The "Light" texture projection (inlet 7 == 6) selects this pair.
//
// Its materials funnel the shading into one channel: materialSpecular is
// (0,0,1), so the specular is blue-only, and pow(dotNH, 0.5) has unbounded
// slope at its terminator. Blue is not bit-reproducible across renders, which
// is why the obj-cube case in tests/integration/ThreedimRenderTest.cpp covers
// "rg" only and asserts the specular by its shape.
const constexpr auto model_display_fragment_shader_phong = R"_(#version 450

)_" model_display_default_uniforms R"_(

layout(binding=3) uniform sampler2D y_tex;

layout(location = 0) in vec3 esVertex;
layout(location = 1) in vec3 esNormal;
layout(location = 2) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

vec4 lightPosition = vec4(100, 10, 10, 0.); // should be in the eye space
vec4 lightAmbient = vec4(0.1, 0.1, 0.1, 1); // light ambient color
vec4 lightDiffuse = vec4(0.0, 0.2, 0.7, 1); // light diffuse color
vec4 lightSpecular = vec4(0.9, 0.9, 0.9, 1); // light specular color
vec4 materialAmbient= vec4(0.1, 0.4, 0, 1); // material ambient color
vec4 materialDiffuse= vec4(0.2, 0.8, 0, 1); // material diffuse color
vec4 materialSpecular= vec4(0, 0, 1, 1); // material specular color
float materialShininess = 0.5; // material specular shininess

void main ()
{
    vec3 normal = normalize(esNormal);

    // Two-sided shading. An OBJ carries whatever normal orientation it was
    // authored with, and a cube presented to this camera turns out to show
    // faces whose normals point AWAY from it -- measured with a signed-normal
    // probe: the three visible faces read -X, -Y and -Z. With a one-sided
    // model every one of them has dot(N, L) < 0 for any light in front of the
    // camera, max(dot, 0) clamps all three to zero, and the shading collapses
    // to ambient alone: lightAmbient * materialAmbient = (0.01, 0.04, 0). A
    // near-black cube with no diffuse and no specular at all.
    //
    // That is what broke the obj-cube golden after the light stopped being
    // animated: the old sin/cos sweep put the light behind the geometry at
    // some phases, so the faces WERE lit part of the time and the golden
    // captured one of those frames. No single static direction can replace
    // that -- pointing the light into the octant those faces face lights this
    // cube and turns two other scenes black instead (measured: their
    // "draws more than 64 lit pixels" checks fail).
    //
    // Flipping the normal toward the viewer fixes the class rather than one
    // scene, and is what a renderer should do with geometry it does not
    // control. Front faces are unaffected -- for them dot(N, view) is already
    // positive.
    vec3 view = normalize(-esVertex);
    if(dot(normal, view) < 0.0)
        normal = -normal;

    vec3 light;
    // The light used to be animated off the transport clock here:
    //     lightPosition.y = sin(TIME) * 20.;
    //     lightPosition.z = cos(TIME) * 50.;
    // Those two lines overrode the initialiser above unconditionally, so its y
    // and z were dead, and there was no inlet to pin the phase or to turn the
    // motion off -- it was not a feature anyone could configure, it just moved.
    //
    // It made a still frame of this shader unreproducible: which frame you get
    // depends on the transport date the grab lands on. The materials funnel it
    // into one channel (materialSpecular is (0,0,1), so the specular is
    // blue-only) through pow(dotNH, 0.5), whose slope is unbounded at the
    // terminator, so a sub-percent rotation swung blue by ~70 codes and the
    // obj-cube golden could not reproduce AGAINST ITSELF: two consecutive
    // renders differed by max_abs 59 over 0.47% of pixels.
    //
    // The light is now what its initialiser says it is. If animation is wanted
    // it belongs on an inlet, where it can be pinned for a still frame.
    if(lightPosition.w == 0.0)
    {
        light = normalize(lightPosition.xyz);
    }
    else
    {
        light = normalize(lightPosition.xyz - esVertex);
    }
    vec3 halfv = normalize(light + view);

    vec3 color = lightAmbient.rgb * materialAmbient.rgb;        // begin with ambient
    float dotNL = max(dot(normal, light), 0.0);
    color += lightDiffuse.rgb * materialDiffuse.rgb * dotNL;    // add diffuse
    // color *= texture2D(map0, texCoord0).rgb;                    // modulate texture map
    float dotNH = max(dot(normal, halfv), 0.0);
    color += pow(dotNH, materialShininess) * lightSpecular.rgb * materialSpecular.rgb; // add specular


    vec4 tex = texture(y_tex, v_texcoord);
    fragColor = vec4(mix(color, tex.rgb, 0.5), materialDiffuse.a);
}
)_";

const constexpr auto model_display_vertex_shader_texcoord = R"_(#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 v_texcoord;

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;

%vtx_define_filters%

%vtx_output%

void main()
{
  vec3 in_position = position;
  vec3 in_normal = vec3(0);
  vec2 in_uv = texcoord;
  vec3 in_tangent = vec3(0);
  vec4 in_color = vec4(1);

  %vtx_do_filters%

#if !defined(QSHADER_SPIRV)
  v_texcoord = in_uv;
#else
  v_texcoord = vec2(in_uv.x, 1. - in_uv.y);
#endif

  %vtx_do_projection%

  gl_Position = renderer.clipSpaceCorrMatrix * v_projected;
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  // Y convention, as in ImageNode: GL is Y-up with no flip, Vulkan's flip is
  // baked into QRhi's clipSpaceCorrMatrix, and D3D/Metal share Vulkan's
  // framebuffer origin without its NDC sign convention. Flip here so the
  // offscreen texture lands top-row-first on every backend, leaving the screen
  // compositor's SPIRV-only UV flip alone.
  gl_Position.y = -gl_Position.y;
#endif

  %vtx_output_process%
}
)_";

const constexpr auto model_display_fragment_shader_texcoord = R"_(#version 450

)_" model_display_default_uniforms R"_(

layout(binding=3) uniform sampler2D y_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main ()
{
  fragColor = texture(y_tex, v_texcoord);
}
)_";

// See also:
// https://www.pbr-book.org/3ed-2018/Texture/Texture_Coordinate_Generation#fragment-TextureMethodDefinitions-3
// https://gamedevelopment.tutsplus.com/articles/use-tri-planar-texture-mapping-for-better-terrain--gamedev-13821

const constexpr auto model_display_vertex_shader_triplanar = R"_(#version 450
layout(location = 0) in vec3 position;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_coords;

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;

%vtx_define_filters%

%vtx_output%

void main()
{
  vec3 in_position = position;
  vec3 in_normal = normal;
  vec2 in_uv = vec2(0);
  vec3 in_tangent = vec3(0);
  vec4 in_color = vec4(1);

  %vtx_do_filters%

  v_normal = in_normal;
  v_coords = (camera.matrixModel * vec4(in_position.xyz, 1.0)).xyz;

  %vtx_do_projection%

  gl_Position = renderer.clipSpaceCorrMatrix * v_projected;
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  // Y convention, as in ImageNode: GL is Y-up with no flip, Vulkan's flip is
  // baked into QRhi's clipSpaceCorrMatrix, and D3D/Metal share Vulkan's
  // framebuffer origin without its NDC sign convention. Flip here so the
  // offscreen texture lands top-row-first on every backend, leaving the screen
  // compositor's SPIRV-only UV flip alone.
  gl_Position.y = -gl_Position.y;
#endif

  %vtx_output_process%
}
)_";

const constexpr auto model_display_fragment_shader_triplanar = R"_(#version 450

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;
#if defined(QSHADER_SPIRV)
vec2 md_uv(vec2 uv) { return vec2(uv.x, 1. - uv.y); }
#else
vec2 md_uv(vec2 uv) { return uv; }
#endif

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_coords;
layout(location = 0) out vec4 fragColor;

void main ()
{
  vec3 blending = abs( v_normal );
  blending = normalize(max(blending, 0.00001)); // Force weights to sum to 1.0
  float b = (blending.x + blending.y + blending.z);
  blending /= vec3(b, b, b);

  float scale = 0.1;

  vec4 xaxis = texture(y_tex, md_uv(v_coords.yz * scale));
  vec4 yaxis = texture(y_tex, md_uv(v_coords.xz * scale));
  vec4 zaxis = texture(y_tex, md_uv(v_coords.xy * scale));
  vec4 tex = xaxis * blending.x + yaxis * blending.y + zaxis * blending.z;

  // Lighting floor: without this the pass emits ONLY the projected texture,
  // so a mesh routed here with no texture wired (the common case for a plain
  // STL/PLY/OBJ that carries normals but no UVs) renders pure black — the
  // geometry is invisible. Shade by the surface normal so the mesh is always
  // visible; a wired texture still dominates via max().
  vec3 N = normalize(v_normal);
  vec3 L = normalize(vec3(0.4, 0.7, 1.0));
  float dif = max(dot(N, L), 0.0);
  vec3 lit = vec3(0.12, 0.16, 0.12) + vec3(0.2, 0.8, 0.0) * dif;

  fragColor = vec4(max(tex.rgb, lit), 1.0);
}
)_";

const constexpr auto model_display_vertex_shader_spherical = R"_(#version 450
layout(location = 0) in vec3 position;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec3 v_e;
layout(location = 1) out vec3 v_n;

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;

%vtx_define_filters%

%vtx_output%

void main()
{
  vec3 in_position = position;
  vec3 in_normal = normal;
  vec2 in_uv = vec2(0);
  vec3 in_tangent = vec3(0);
  vec4 in_color = vec4(1);

  %vtx_do_filters%

  // https://www.clicktorelease.com/blog/creating-spherical-environment-mapping-shader.html
  vec4 p = vec4( in_position, 1. );
  v_e = normalize( vec3( camera.matrixModelView * p ) );
  v_n = normal; //normalize( camera.matrixNormal * in_normal );

  %vtx_do_projection%

  gl_Position = renderer.clipSpaceCorrMatrix * v_projected;
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  // Y convention, as in ImageNode: GL is Y-up with no flip, Vulkan's flip is
  // baked into QRhi's clipSpaceCorrMatrix, and D3D/Metal share Vulkan's
  // framebuffer origin without its NDC sign convention. Flip here so the
  // offscreen texture lands top-row-first on every backend, leaving the screen
  // compositor's SPIRV-only UV flip alone.
  gl_Position.y = -gl_Position.y;
#endif

  %vtx_output_process%
}
)_";
const constexpr auto model_display_fragment_shader_spherical = R"_(#version 450

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;
#if defined(QSHADER_SPIRV)
vec2 md_uv(vec2 uv) { return vec2(uv.x, 1. - uv.y); }
#else
vec2 md_uv(vec2 uv) { return uv; }
#endif

layout(location = 0) in vec3 v_e;
layout(location = 1) in vec3 v_n;
layout(location = 0) out vec4 fragColor;

void main ()
{
  vec2 uv = vec2(atan(v_n.z, v_n.x), asin(v_n.y));
  uv = uv * vec2(1. / (2. * 3.14159265358979323846264338327), 1. / 3.14159265358979323846264338327) + 0.5;
  fragColor = texture(y_tex, md_uv(uv));
}
)_";

const constexpr auto model_display_vertex_shader_spherical2 = R"_(#version 450
layout(location = 0) in vec3 position;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec3 v_e;
layout(location = 1) out vec3 v_n;

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;

%vtx_define_filters%

%vtx_output%

void main()
{
  vec3 in_position = position;
  vec3 in_normal = normal;
  vec2 in_uv = vec2(0);
  vec3 in_tangent = vec3(0);
  vec4 in_color = vec4(1);

  %vtx_do_filters%

  // https://www.clicktorelease.com/blog/creating-spherical-environment-mapping-shader.html
  vec4 p = vec4( in_position, 1. );
  v_e = normalize( vec3( camera.matrixModelView * p ) );
  v_n = normalize( camera.matrixNormal * in_normal );

  %vtx_do_projection%

  gl_Position = renderer.clipSpaceCorrMatrix * v_projected;
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  // Match the codebase Y-handling convention used by ImageNode et al.:
  // GL is Y-up framebuffer (no flip), Vulkan's Y flip is baked into
  // QRhi's clipSpaceCorrMatrix, but D3D/Metal share Vulkan's framebuffer
  // origin without sharing its NDC sign convention — so we flip here so
  // the offscreen texture lands top-row-first like the other backends,
  // and the screen compositor (ScaledRenderer) keeps its SPIRV-only UV
  // flip.
  gl_Position.y = -gl_Position.y;
#endif

  %vtx_output_process%
}
)_";
const constexpr auto model_display_fragment_shader_spherical2 = R"_(#version 450

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;
#if defined(QSHADER_SPIRV)
vec2 md_uv(vec2 uv) { return vec2(uv.x, 1. - uv.y); }
#else
vec2 md_uv(vec2 uv) { return uv; }
#endif

layout(location = 0) in vec3 v_e;
layout(location = 1) in vec3 v_n;
layout(location = 0) out vec4 fragColor;

void main ()
{
  vec3 r = reflect( v_e, v_n );
  float m = 2. * sqrt( pow( r.x, 2. ) + pow( r.y, 2. ) + pow( r.z + 1., 2. ) );
  vec2 vN = r.xy / m + .5;

  fragColor = texture(y_tex, md_uv(vN.xy));
}
)_";

const constexpr auto model_display_vertex_shader_viewspace = R"_(#version 450
layout(location = 0) in vec3 position;

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;

%vtx_define_filters%

%vtx_output%

void main()
{
  vec3 in_position = position;
  vec3 in_normal = vec3(0);
  vec2 in_uv = vec2(0);
  vec3 in_tangent = vec3(0);
  vec4 in_color = vec4(1);

  %vtx_do_filters%

  %vtx_do_projection%

  gl_Position = renderer.clipSpaceCorrMatrix * v_projected;
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  // Match the codebase Y-handling convention used by ImageNode et al.:
  // GL is Y-up framebuffer (no flip), Vulkan's Y flip is baked into
  // QRhi's clipSpaceCorrMatrix, but D3D/Metal share Vulkan's framebuffer
  // origin without sharing its NDC sign convention — so we flip here so
  // the offscreen texture lands top-row-first like the other backends,
  // and the screen compositor (ScaledRenderer) keeps its SPIRV-only UV
  // flip.
  gl_Position.y = -gl_Position.y;
#endif

  %vtx_output_process%
}
)_";

const constexpr auto model_display_fragment_shader_viewspace = R"_(#version 450

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;

layout(location = 0) out vec4 fragColor;

void main ()
{
  fragColor = texture(y_tex, gl_FragCoord.xy / renderer.renderSize.xy);
}
)_";

const constexpr auto model_display_vertex_shader_barycentric = R"_(#version 450
layout(location = 0) in vec3 position;

layout(location = 1) out vec2 v_bary;

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;

%vtx_define_filters%

%vtx_output%

void main()
{
  vec3 in_position = position;
  vec3 in_normal = vec3(0);
  vec2 in_uv = vec2(0);
  vec3 in_tangent = vec3(0);
  vec4 in_color = vec4(1);

  %vtx_do_filters%

  if(gl_VertexIndex % 3 == 0) v_bary = vec2(0, 0);
  else if(gl_VertexIndex % 3 == 1) v_bary = vec2(0, 1);
  else if(gl_VertexIndex % 3 == 2) v_bary = vec2(1, 0);

  %vtx_do_projection%

  gl_Position = renderer.clipSpaceCorrMatrix * v_projected;
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  // Match the codebase Y-handling convention used by ImageNode et al.:
  // GL is Y-up framebuffer (no flip), Vulkan's Y flip is baked into
  // QRhi's clipSpaceCorrMatrix, but D3D/Metal share Vulkan's framebuffer
  // origin without sharing its NDC sign convention — so we flip here so
  // the offscreen texture lands top-row-first like the other backends,
  // and the screen compositor (ScaledRenderer) keeps its SPIRV-only UV
  // flip.
  gl_Position.y = -gl_Position.y;
#endif

  %vtx_output_process%
}
)_";

const constexpr auto model_display_fragment_shader_barycentric = R"_(#version 450

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;
#if defined(QSHADER_SPIRV)
vec2 md_uv(vec2 uv) { return vec2(uv.x, 1. - uv.y); }
#else
vec2 md_uv(vec2 uv) { return uv; }
#endif

layout(location = 1) in vec2 v_bary;
layout(location = 0) out vec4 fragColor;

void main ()
{
  fragColor = texture(y_tex, md_uv(v_bary));
}
)_";

const constexpr auto model_display_vertex_shader_color = R"_(#version 450
layout(location = 0) in vec3 position;
layout(location = 2) in vec3 color;

layout(location = 0) out vec3 v_color;

)_" model_display_default_uniforms R"_(

%vtx_define_filters%

%vtx_output%

void main()
{
  vec3 in_position = position;
  vec3 in_normal = vec3(0);
  vec2 in_uv = vec2(0);
  vec3 in_tangent = vec3(0);
  vec4 in_color = vec4(color.rgb, 1.);

  %vtx_do_filters%

  v_color = in_color.rgb;

  %vtx_do_projection%

  gl_Position = renderer.clipSpaceCorrMatrix * v_projected;
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  // Match the codebase Y-handling convention used by ImageNode et al.:
  // GL is Y-up framebuffer (no flip), Vulkan's Y flip is baked into
  // QRhi's clipSpaceCorrMatrix, but D3D/Metal share Vulkan's framebuffer
  // origin without sharing its NDC sign convention — so we flip here so
  // the offscreen texture lands top-row-first like the other backends,
  // and the screen compositor (ScaledRenderer) keeps its SPIRV-only UV
  // flip.
  gl_Position.y = -gl_Position.y;
#endif

  %vtx_output_process%
}
)_";

const constexpr auto model_display_fragment_shader_color = R"_(#version 450

)_" model_display_default_uniforms R"_(

layout(location = 0) in vec3 v_color;
layout(location = 0) out vec4 fragColor;

void main ()
{
  fragColor.rgb = v_color;
  fragColor.a = 1.;
}
)_";

// The guides: lines in world space, and the background in clip space
const constexpr auto model_display_guide_vertex_shader = R"_(#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 v_color;

)_" model_display_default_uniforms R"_(

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_color = color;
  gl_Position = renderer.clipSpaceCorrMatrix * camera.matrixProjection * camera.matrixView * vec4(position, 1.0);
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = -gl_Position.y;
#endif
}
)_";

const constexpr auto model_display_guide_background_vertex_shader = R"_(#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 v_color;

)_" model_display_default_uniforms R"_(

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_color = color;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.0);
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = -gl_Position.y;
#endif
}
)_";

const constexpr auto model_display_guide_fragment_shader = R"_(#version 450
layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 fragColor;

void main()
{
  fragColor = v_color;
}
)_";

ModelDisplayNode::ModelDisplayNode()
{
  this->requiresDepth = true;

  input.push_back(new Port{this, nullptr, Types::Image, {}});
  input.push_back(new Port{this, nullptr, Types::Geometry, {}});
  input.push_back(new Port{this, nullptr, Types::Camera, {}});
  output.push_back(new Port{this, {}, Types::Image, {}});

  m_materialData.reset((char*)&ubo);
}

ModelDisplayNode::~ModelDisplayNode()
{
  m_materialData.release();
}

namespace
{
// Bounds of positions on the GPU: floats mapped to unsigned integers in the
// same order, for atomic min and max
const constexpr auto model_display_bounds_shader = R"_(#version 450
layout(local_size_x = 256) in;

layout(std140, binding = 0) uniform Params
{
  uint count;
  uint first;  // floats
  uint stride; // floats
  uint pad;
};
layout(std430, binding = 1) readonly buffer Positions { float positions[]; };
layout(std430, binding = 2) buffer Box { uint lo[3]; uint hi[3]; };

uint ordered(float f)
{
  const uint u = floatBitsToUint(f);
  return (u & 0x80000000u) != 0u ? ~u : (u | 0x80000000u);
}

void main()
{
  const uint i = gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * gl_NumWorkGroups.x * 256u;
  if(i >= count)
    return;
  const uint at = first + i * stride;
  const vec3 p = vec3(positions[at], positions[at + 1u], positions[at + 2u]);
  if(any(isnan(p)) || any(isinf(p)))
    return;
  for(int k = 0; k < 3; k++)
  {
    atomicMin(lo[k], ordered(p[k]));
    atomicMax(hi[k], ordered(p[k]));
  }
}
)_";

float unordered(uint32_t u)
{
  const uint32_t bits = (u & 0x80000000u) ? (u & 0x7fffffffu) : ~u;
  float f;
  std::memcpy(&f, &bits, sizeof(f));
  return f;
}

//! Bounds of the geometries, from their producers or else from positions held
//! in memory; empty when neither tells
ossia::aabb measureBounds(const ossia::geometry_spec& spec) noexcept
{
  ossia::aabb box{{1.f, 1.f, 1.f}, {-1.f, -1.f, -1.f}};
  if(!spec.meshes)
    return box;
  for(const auto& g : spec.meshes->meshes)
  {
    const auto& b = g.bounds;
    if(b.max[0] > b.min[0] || b.max[1] > b.min[1] || b.max[2] > b.min[2])
    {
      box.expand(b.min[0], b.min[1], b.min[2]);
      box.expand(b.max[0], b.max[1], b.max[2]);
      continue;
    }
    if(const auto p = geometryPositionBounds(g); !p.empty())
    {
      box.expand(p.min[0], p.min[1], p.min[2]);
      box.expand(p.max[0], p.max[1], p.max[2]);
    }
  }
  return box;
}
}

class ModelDisplayNode::Renderer : public GenericNodeRenderer
{
public:
  using GenericNodeRenderer::GenericNodeRenderer;

  struct RenderShaders
  {
    QShader phongVS, phongFS;
    QShader texCoordVS, texCoordFS;
    QShader triplanarVS, triplanarFS;
    QShader sphericalVS, sphericalFS;
    QShader spherical2VS, spherical2FS;
    QShader viewspaceVS, viewspaceFS;
    QShader barycentricVS, barycentricFS;
    QShader colorVS, colorFS;
  };

  // Camera mode enum — matches the UI ordering. Index into
  // triangle_shaders / point_shaders arrays.
  //
  //   0 = Perspective
  //   1 = Fulldome (Equidistant, domemaster)
  //   2 = Fulldome (Equisolid-angle, photographic fisheye)
  //   3 = Fulldome (Stereographic, conformal)
  //   4 = Fulldome (Orthographic, ≤ 180° only)
  static constexpr int CAMERA_MODE_COUNT = 5;
  RenderShaders triangle_shaders[CAMERA_MODE_COUNT];
  RenderShaders point_shaders[CAMERA_MODE_COUNT];

  int64_t meshChangedIndex{-1};
  // The geometry filters whose material UBOs are baked into m_p's SRBs, as
  // (node id, material generation). Those UBOs belong to the filters'
  // renderers: when one is released or recreated (a live graph edit, outside
  // a frame, deletes it at once) the SRBs must be rebuilt before the next draw.
  ossia::small_vector<std::pair<int64_t, uint64_t>, 8> m_boundFilters;
  bool m_inputRemoved{false};

  static uint64_t filterGeneration(RenderList& renderer, int64_t node_id)
  {
    for(auto n : renderer.renderers)
    {
      if(n->nodeId == node_id)
      {
        if(auto c = dynamic_cast<score::gfx::GeometryFilterNodeRenderer*>(n))
          return c->materialGeneration();
        break;
      }
    }
    return 0;
  }

  void bindFilterSignature(RenderList& renderer, const Mesh& mesh)
  {
    m_boundFilters.clear();
    if(!mesh.filters)
      return;
    for(auto& f : mesh.filters->filters)
      m_boundFilters.emplace_back(f.node_id, filterGeneration(renderer, f.node_id));
  }

  //! Checked every frame: compares in place rather than building a signature.
  bool filtersStillBound(RenderList& renderer, const Mesh& mesh) const
  {
    const std::size_t n = mesh.filters ? mesh.filters->filters.size() : 0;
    if(n != m_boundFilters.size())
      return false;
    for(std::size_t i = 0; i < n; i++)
    {
      const auto& f = mesh.filters->filters[i];
      if(m_boundFilters[i].first != f.node_id
         || m_boundFilters[i].second != filterGeneration(renderer, f.node_id))
        return false;
    }
    return true;
  }

  int m_curShader{0};
  //! The fallback patterns of an unconnected texture inlet, by pattern
  std::array<QRhiTexture*, 3> m_patterns{};
  //! Created and not uploaded yet
  std::array<bool, 3> m_patternPending{};
  int m_draw_mode{0};
  int m_camera_mode{0};
  int m_blend_color_src{0};
  int m_blend_color_dst{0};
  int m_blend_color_op{0};
  int m_blend_alpha_src{0};
  int m_blend_alpha_dst{0};
  int m_blend_alpha_op{0};
  bool m_blend_enabled{false};
  bool m_prefer_vertex_colors{false};
  // The render size the projection's aspect ratio was computed for.
  QSize m_projectionSize{};

  //! The bounds of a geometry held only on the GPU, measured by a compute
  //! pass and read back a few frames later
  struct BoundsMeasure
  {
    QShader shader;
    QRhiComputePipeline* pipeline{};
    QRhiShaderResourceBindings* srb{};
    QRhiBuffer* params{};
    QRhiBuffer* box{};
    QRhiBuffer* source{};
    QRhiBufferReadbackResult result;
    bool failed{};
    bool requested{};
    bool inFlight{};
  } m_bounds;
  int64_t m_fitRequest{-1};
  int64_t m_boundsMeshIndex{-1};

  void releaseBounds(RenderList& renderer)
  {
    // The readback writes into the result when its frame completes
    if(m_bounds.inFlight && renderer.state.rhi)
      renderer.state.rhi->finish();
    delete m_bounds.pipeline;
    delete m_bounds.srb;
    delete m_bounds.params;
    delete m_bounds.box;
    m_bounds.pipeline = nullptr;
    m_bounds.srb = nullptr;
    m_bounds.params = nullptr;
    m_bounds.box = nullptr;
    m_bounds.source = nullptr;
    m_bounds.inFlight = false;
    m_bounds.requested = false;
  }

  //! Draws the scene in full, for the nodes that have one
  std::unique_ptr<ModelSceneDrawer> m_scene;
  //! The camera of the last frame, for the scene drawer
  ModelSceneView m_view;

  bool drawsScene() const noexcept { return m_scene && m_scene->active(); }

  //! Measures in memory when it can, else asks the compute pass
  void measure(ModelDisplayNode& n)
  {
    // A scene: the bounds of everything it draws, where it draws it
    if(drawsScene())
    {
      m_bounds.requested = false;
      if(const auto& box = m_scene->bounds(); !box.empty() && n.fitBounds(box))
        this->materialChanged = true;
      return;
    }
    if(const auto box = measureBounds(this->geometry); !box.empty())
    {
      m_bounds.requested = false;
      if(n.fitBounds(box))
        this->materialChanged = true;
    }
    else
      m_bounds.requested = true;
  }

  //! A result of the compute pass, once its frame completed
  void collectBounds(ModelDisplayNode& n)
  {
    if(!m_bounds.inFlight || m_bounds.result.data.size() < qsizetype(6 * sizeof(uint32_t)))
      return;
    m_bounds.inFlight = false;
    uint32_t words[6];
    std::memcpy(words, m_bounds.result.data.constData(), sizeof(words));
    m_bounds.result.data.clear();
    ossia::aabb box{
        {unordered(words[0]), unordered(words[1]), unordered(words[2])},
        {unordered(words[3]), unordered(words[4]), unordered(words[5])}};
    // Nothing finite was found: the box stays at its reset values
    if(words[0] == 0xffffffffu || box.empty())
      return;
    if(n.fitBounds(box))
      this->materialChanged = true;
  }

  //! The positions of the geometry as the compute pass reads them: the buffer,
  //! then the first float, the stride in floats and the count
  struct PositionSource
  {
    QRhiBuffer* buffer{};
    uint32_t first{}, stride{}, count{};
  };
  PositionSource gpuPositions() const noexcept
  {
    const auto& spec = this->geometry;
    // The buffers of the mesh follow those of a single geometry
    if(!spec.meshes || spec.meshes->meshes.size() != 1)
      return {};
    const auto& g = spec.meshes->meshes.front();
    using attribute = ossia::geometry::attribute;
    auto attr = ossia::find_if(g.attributes, [](const attribute& a) {
      return a.semantic == ossia::attribute_semantic::position;
    });
    if(attr == g.attributes.end())
      attr = ossia::find_if(g.attributes, [](const attribute& a) { return a.location == 0; });
    if(attr == g.attributes.end()
       || (attr->format != attribute::float3 && attr->format != attribute::float4)
       || attr->binding < 0 || attr->binding >= int(g.input.size())
       || attr->binding >= int(g.bindings.size()))
      return {};
    const auto& in = g.input[attr->binding];
    if(in.buffer < 0 || in.buffer >= int(m_meshbufs.buffers.size()))
      return {};
    QRhiBuffer* buf = m_meshbufs.buffers[in.buffer].handle;
    if(!buf || !buf->usage().testFlag(QRhiBuffer::StorageBuffer)
       || buf->type() == QRhiBuffer::Dynamic)
      return {};
    const int64_t element = attr->format == attribute::float3 ? 12 : 16;
    const int64_t stride = g.bindings[attr->binding].byte_stride > 0
                               ? g.bindings[attr->binding].byte_stride
                               : element;
    const int64_t first = in.byte_offset + attr->byte_offset;
    if(first % 4 != 0 || stride % 4 != 0 || first < 0)
      return {};
    const int64_t size = buf->size();
    int64_t count = g.vertices;
    if(first + 12 > size)
      return {};
    count = std::min<int64_t>(count, (size - first - 12) / stride + 1);
    if(count <= 0)
      return {};
    return {buf, uint32_t(first / 4), uint32_t(stride / 4), uint32_t(count)};
  }

  bool ensureBoundsPipeline(RenderList& renderer)
  {
    if(m_bounds.pipeline)
      return true;
    if(m_bounds.failed)
      return false;
    auto& rhi = *renderer.state.rhi;
    if(!rhi.isFeatureSupported(QRhi::Compute))
    {
      m_bounds.failed = true;
      return false;
    }
    try
    {
      m_bounds.shader = score::gfx::makeCompute(renderer.state, model_display_bounds_shader);
    }
    catch(const std::exception& e)
    {
      qWarning() << "ModelDisplay: the bounds shader did not compile:" << e.what();
      m_bounds.failed = true;
      return false;
    }
    m_bounds.params = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16);
    m_bounds.params->setName("ModelDisplay::bounds_params");
    m_bounds.box = rhi.newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, 24);
    m_bounds.box->setName("ModelDisplay::bounds");
    if(!m_bounds.params->create() || !m_bounds.box->create())
    {
      m_bounds.failed = true;
      return false;
    }
    m_bounds.pipeline = rhi.newComputePipeline();
    m_bounds.pipeline->setShaderStage({QRhiShaderStage::Compute, m_bounds.shader});
    return true;
  }

  void runInitialPasses(
      RenderList& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& res,
      Edge& edge) override
  {
    if(!m_bounds.requested || m_bounds.inFlight || drawsScene())
      return;
    const auto src = gpuPositions();
    if(!src.buffer || !ensureBoundsPipeline(renderer))
      return;
    auto& rhi = *renderer.state.rhi;

    if(m_bounds.source != src.buffer || !m_bounds.srb)
    {
      delete m_bounds.srb;
      m_bounds.srb = rhi.newShaderResourceBindings();
      m_bounds.srb->setBindings(
          {QRhiShaderResourceBinding::uniformBuffer(
               0, QRhiShaderResourceBinding::ComputeStage, m_bounds.params),
           QRhiShaderResourceBinding::bufferLoad(
               1, QRhiShaderResourceBinding::ComputeStage, src.buffer),
           QRhiShaderResourceBinding::bufferLoadStore(
               2, QRhiShaderResourceBinding::ComputeStage, m_bounds.box)});
      if(!m_bounds.srb->create())
        return;
      m_bounds.source = src.buffer;
      if(!m_bounds.pipeline->shaderResourceBindings())
      {
        m_bounds.pipeline->setShaderResourceBindings(m_bounds.srb);
        if(!m_bounds.pipeline->create())
        {
          qWarning() << "ModelDisplay: the bounds pipeline could not be created";
          delete m_bounds.pipeline;
          m_bounds.pipeline = nullptr;
          m_bounds.failed = true;
          return;
        }
      }
    }

    if(!res)
      res = rhi.nextResourceUpdateBatch();
    const uint32_t params[4]{src.count, src.first, src.stride, 0};
    res->updateDynamicBuffer(m_bounds.params, 0, sizeof(params), params);
    const uint32_t reset[6]{0xffffffffu, 0xffffffffu, 0xffffffffu, 0u, 0u, 0u};
    res->uploadStaticBuffer(m_bounds.box, 0, sizeof(reset), reset);

    // Wider than a dimension allows: the rows of groups go on y
    const int64_t groups = (int64_t(src.count) + 255) / 256;
    const int64_t maxDim = std::max(1, rhi.resourceLimit(QRhi::MaxThreadGroupsPerDimension));
    const int x = int(std::min<int64_t>(groups, maxDim));
    const int y = int((groups + x - 1) / x);

    const bool gl = rhi.backend() == QRhi::OpenGLES2;
    cb.beginComputePass(
        res, gl ? QRhiCommandBuffer::BeginPassFlag::ExternalContent
                : QRhiCommandBuffer::BeginPassFlags{});
    res = nullptr;
    cb.setComputePipeline(m_bounds.pipeline);
    cb.setShaderResources(m_bounds.srb);
    cb.dispatch(x, y, 1);
    if(gl)
    {
      cb.beginExternal();
      score::gfx::insertComputeBarrier(rhi, cb);
      cb.endExternal();
    }
    auto* readback = rhi.nextResourceUpdateBatch();
    m_bounds.result.data.clear();
    readback->readBackBuffer(m_bounds.box, 0, 24, &m_bounds.result);
    cb.endComputePass(readback);
    m_bounds.requested = false;
    m_bounds.inFlight = true;
  }

  //! The background, the plane and the axes, drawn in the passes of the model
  struct GuidePass
  {
    Edge* edge{};
    QRhiRenderPassDescriptor* renderPass{};
    QRhiShaderResourceBindings* srb{};
    QRhiGraphicsPipeline* background{};
    QRhiGraphicsPipeline* lines{};

    void release()
    {
      delete background;
      delete lines;
      delete srb;
      *this = {};
    }
  };
  ossia::small_vector<GuidePass, 2> m_guidePasses;
  QRhiBuffer* m_guideVertices{};
  //! The background quad, then the lines
  static constexpr int guide_background_vertices = 4;
  static constexpr int guide_grid_cells = 10;
  static constexpr int guide_max_vertices
      = guide_background_vertices + 2 * 2 * (2 * guide_grid_cells + 1) + 2 * 6;
  static constexpr int guide_vertex_floats = 7;
  int m_guideLineVertices{};
  std::optional<ModelDisplayNode::Guides> m_guidesUploaded;
  QShader m_guideVS, m_guideBackgroundVS, m_guideFS;

  void releaseGuides()
  {
    for(auto& g : m_guidePasses)
      g.release();
    m_guidePasses.clear();
    delete m_guideVertices;
    m_guideVertices = nullptr;
    m_guidesUploaded.reset();
    m_guideLineVertices = 0;
  }

  void uploadGuides(const ModelDisplayNode::Guides& g, QRhiResourceUpdateBatch& res)
  {
    std::array<float, guide_max_vertices * guide_vertex_floats> v{};
    int k = 0;
    auto vertex = [&](float x, float y, float z, const std::array<float, 4>& c) {
      float* p = v.data() + k * guide_vertex_floats;
      p[0] = x, p[1] = y, p[2] = z;
      std::copy(c.begin(), c.end(), p + 3);
      k++;
    };
    vertex(-1, 1, 0, g.top);
    vertex(1, 1, 0, g.top);
    vertex(-1, -1, 0, g.bottom);
    vertex(1, -1, 0, g.bottom);

    const float s = g.spacing > 0.f ? g.spacing : 1.f;
    constexpr int n = guide_grid_cells;
    if(g.plane)
    {
      // Snapped to the cells, so that the grid stays put when the origin moves
      const float ox = std::round(g.origin[0] / s) * s, oz = std::round(g.origin[2] / s) * s;
      const float y = g.origin[1];
      for(int i = -n; i <= n; i++)
      {
        const std::array<float, 4> c{0.6f, 0.6f, 0.6f, i % 5 == 0 ? 0.45f : 0.2f};
        vertex(ox + i * s, y, oz - n * s, c);
        vertex(ox + i * s, y, oz + n * s, c);
        vertex(ox - n * s, y, oz + i * s, c);
        vertex(ox + n * s, y, oz + i * s, c);
      }
    }
    if(g.axes)
    {
      const float len = n * s;
      const std::array<float, 3> colors[3]{
          {0.92f, 0.27f, 0.24f}, {0.35f, 0.78f, 0.35f}, {0.31f, 0.55f, 0.94f}};
      for(int a = 0; a < 3; a++)
      {
        const auto& c = colors[a];
        float e[3]{};
        e[a] = len;
        vertex(0, 0, 0, {c[0], c[1], c[2], 1.f});
        vertex(e[0], e[1], e[2], {c[0], c[1], c[2], 1.f});
        vertex(0, 0, 0, {c[0], c[1], c[2], 0.35f});
        vertex(-e[0], -e[1], -e[2], {c[0], c[1], c[2], 0.35f});
      }
    }
    m_guideLineVertices = k - guide_background_vertices;
    res.updateDynamicBuffer(m_guideVertices, 0, k * guide_vertex_floats * sizeof(float), v.data());
  }

  QRhiGraphicsPipeline* makeGuidePipeline(
      RenderList& renderer, const TextureRenderTarget& rt, QRhiShaderResourceBindings* srb,
      bool background)
  {
    auto& rhi = *renderer.state.rhi;
    auto* ps = rhi.newGraphicsPipeline();
    ps->setName("ModelDisplay::guides");
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = !background;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    ps->setTargetBlends({blend});
    const int samples = rt.sampleCount();
    ps->setSampleCount(samples > 0 ? samples : renderer.samples());
    ps->setTopology(
        background ? QRhiGraphicsPipeline::TriangleStrip : QRhiGraphicsPipeline::Lines);
    // Behind everything; the lines are hidden by the model but do not hide it
    const bool depth = !background
                       && (rt.depthTexture || rt.depthRenderBuffer || rt.msDepthTexture);
    ps->setDepthTest(depth);
    ps->setDepthWrite(false);
    ps->setDepthOp(QRhiGraphicsPipeline::Greater);
    ps->setShaderStages(
        {{QRhiShaderStage::Vertex, background ? m_guideBackgroundVS : m_guideVS},
         {QRhiShaderStage::Fragment, m_guideFS}});
    QRhiVertexInputLayout layout;
    layout.setBindings({{guide_vertex_floats * sizeof(float)}});
    layout.setAttributes(
        {{0, 0, QRhiVertexInputAttribute::Float3, 0},
         {0, 1, QRhiVertexInputAttribute::Float4, 3 * sizeof(float)}});
    ps->setVertexInputLayout(layout);
    ps->setShaderResourceBindings(srb);
    ps->setRenderPassDescriptor(rt.renderPass);
    if(!ps->create())
    {
      delete ps;
      return nullptr;
    }
    return ps;
  }

  //! The guides follow the passes of the model
  void updateGuides(RenderList& renderer, QRhiResourceUpdateBatch& res)
  {
    const auto& g = static_cast<const ModelDisplayNode&>(this->node).guides;
    if(!g.any() && !(drawsScene() && m_scene->hasLines()))
    {
      if(m_guideVertices)
        releaseGuides();
      return;
    }

    auto& rhi = *renderer.state.rhi;
    if(!m_guideFS.isValid())
    {
      std::tie(m_guideVS, m_guideFS) = score::gfx::makeShaders(
          renderer.state, model_display_guide_vertex_shader,
          model_display_guide_fragment_shader);
      m_guideBackgroundVS = score::gfx::makeShaders(
                                renderer.state, model_display_guide_background_vertex_shader,
                                model_display_guide_fragment_shader)
                                .first;
    }
    if(!m_guideVertices)
    {
      m_guideVertices = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
          guide_max_vertices * guide_vertex_floats * sizeof(float));
      m_guideVertices->setName("ModelDisplay::guides");
      if(!m_guideVertices->create())
      {
        delete m_guideVertices;
        m_guideVertices = nullptr;
        return;
      }
    }
    if(m_guidesUploaded != g)
    {
      uploadGuides(g, res);
      m_guidesUploaded = g;
    }

    // Pipelines for the passes the model has, on their render pass
    for(auto it = m_guidePasses.begin(); it != m_guidePasses.end();)
    {
      auto pass = ossia::find_if(m_p, [&](const auto& p) { return p.first == it->edge; });
      if(pass == m_p.end() || pass->second.renderTarget.renderPass != it->renderPass)
      {
        it->release();
        it = m_guidePasses.erase(it);
      }
      else
        ++it;
    }
    for(auto& [edge, pass] : m_p)
    {
      if(ossia::any_of(m_guidePasses, [e = edge](const auto& gp) { return gp.edge == e; }))
        continue;
      const auto& rt = pass.renderTarget;
      if(!rt.renderPass || !m_material.buffer)
        continue;
      GuidePass gp{.edge = edge, .renderPass = rt.renderPass};
      gp.srb = score::gfx::createDefaultBindings(
          renderer, rt, m_processUBO, m_material.buffer, {});
      gp.background = makeGuidePipeline(renderer, rt, gp.srb, true);
      gp.lines = makeGuidePipeline(renderer, rt, gp.srb, false);
      m_guidePasses.push_back(gp);
    }
  }

  void drawGuides(
      RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge, bool background)
  {
    if(!m_guideVertices || !m_guidesUploaded)
      return;
    auto gp = ossia::find_if(m_guidePasses, [&](const auto& g) { return g.edge == &edge; });
    if(gp == m_guidePasses.end())
      return;
    const auto& g = *m_guidesUploaded;
    QRhiGraphicsPipeline* ps = background ? gp->background : gp->lines;
    const int count = background ? (g.top[3] > 0.f || g.bottom[3] > 0.f
                                        ? guide_background_vertices
                                        : 0)
                                 : m_guideLineVertices;
    if(!ps || count == 0)
      return;
    const auto sz = renderer.renderSize(&edge);
    cb.setGraphicsPipeline(ps);
    cb.setShaderResources(gp->srb);
    cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));
    const QRhiCommandBuffer::VertexInput input{m_guideVertices, 0};
    cb.setVertexInput(0, 1, &input);
    cb.draw(count, 1, background ? 0 : guide_background_vertices, 0);
  }

private:
  ~Renderer() = default;

  score::gfx::RenderList* m_renderer{};

  void initPasses_impl(RenderList& renderer, const Mesh& mesh, RenderShaders& shaders)
  {
    auto& n = (ModelDisplayNode&)node;
    bool has_texcoord = mesh.flags() & Mesh::HasTexCoord;
    bool has_normals = mesh.flags() & Mesh::HasNormals;
    bool has_colors = mesh.flags() & Mesh::HasColor;

    int cur_binding = 4;
    std::vector<QRhiBuffer*> ubos;
    std::vector<QRhiShaderResourceBinding> additional_bindings;
    bindFilterSignature(renderer, mesh);

    if (mesh.filters)
    {
      if (!mesh.filters->filters.empty())
      {
        for (auto& f : mesh.filters->filters)
        {
          for (auto n : renderer.renderers)
          {
            if (n->nodeId == f.node_id)
            {
              if (auto c = safe_cast<score::gfx::GeometryFilterNodeRenderer*>(n))
              {
                if(auto mat = c->material())
                {
                  additional_bindings.push_back(QRhiShaderResourceBinding::uniformBuffer(
                      cur_binding, QRhiShaderResourceBinding::VertexStage, mat));
                }
              }
              break;
            }
          }
          // One binding slot per filter, matching the shader generator
          // which advances %next% per filter unconditionally — a filter
          // whose renderer isn't resolved yet must not shift every
          // subsequent filter's UBO binding.
          cur_binding++;
        }
      }
    }

    if(has_colors && (n.texture_projection == 7 || n.prefer_vertex_colors))
    {
      defaultPassesInit(
          renderer, mesh, shaders.colorVS, shaders.colorFS, additional_bindings);
      return;
    }

    if (has_texcoord && has_normals)
    {
      switch(n.texture_projection)
      {
        default:
        case 0: // Needs TCoord
          defaultPassesInit(
              renderer,
              mesh,
              shaders.texCoordVS,
              shaders.texCoordFS,
              additional_bindings);
          break;
        case 1: // Needs Normals
          defaultPassesInit(
              renderer,
              mesh,
              shaders.triplanarVS,
              shaders.triplanarFS,
              additional_bindings);
          break;
        case 2: // Needs Normals
          defaultPassesInit(
              renderer,
              mesh,
              shaders.sphericalVS,
              shaders.sphericalFS,
              additional_bindings);
          break;
        case 3: // Needs Normals
          defaultPassesInit(
              renderer,
              mesh,
              shaders.spherical2VS,
              shaders.spherical2FS,
              additional_bindings);
          break;
        case 4: // Needs just position
          defaultPassesInit(
              renderer,
              mesh,
              shaders.viewspaceVS,
              shaders.viewspaceFS,
              additional_bindings);
          break;
        case 5: // Needs just position
          defaultPassesInit(
              renderer,
              mesh,
              shaders.barycentricVS,
              shaders.barycentricFS,
              additional_bindings);
          break;
        case 6: // Needs TCoord + Normals
          defaultPassesInit(
              renderer, mesh, shaders.phongVS, shaders.phongFS, additional_bindings);
          break;
      }
    }
    else if (has_texcoord && !has_normals)
    {
      switch(n.texture_projection)
      {
        default:
        case 0:
        case 1:
        case 2:
        case 3:
        case 6:
          defaultPassesInit(
              renderer,
              mesh,
              shaders.texCoordVS,
              shaders.texCoordFS,
              additional_bindings);
          break;
        case 4: // Needs just position
          defaultPassesInit(
              renderer,
              mesh,
              shaders.viewspaceVS,
              shaders.viewspaceFS,
              additional_bindings);
          break;
        case 5: // Needs just position
          defaultPassesInit(
              renderer,
              mesh,
              shaders.barycentricVS,
              shaders.barycentricFS,
              additional_bindings);
          break;
      }
    }
    else if (has_normals && !has_texcoord)
    {
      switch(n.texture_projection)
      {
        default:
        case 0:
        case 6:
        case 1: // Needs Normals
          defaultPassesInit(
              renderer,
              mesh,
              shaders.triplanarVS,
              shaders.triplanarFS,
              additional_bindings);
          break;
        case 2: // Needs Normals
          defaultPassesInit(
              renderer,
              mesh,
              shaders.sphericalVS,
              shaders.sphericalFS,
              additional_bindings);
          break;
        case 3: // Needs Normals
          defaultPassesInit(
              renderer,
              mesh,
              shaders.spherical2VS,
              shaders.spherical2FS,
              additional_bindings);
          break;
        case 4: // Needs just position
          defaultPassesInit(
              renderer,
              mesh,
              shaders.viewspaceVS,
              shaders.viewspaceFS,
              additional_bindings);
          break;
        case 5: // Needs just position
          defaultPassesInit(
              renderer,
              mesh,
              shaders.barycentricVS,
              shaders.barycentricFS,
              additional_bindings);
          break;
      }
    }
    else if (!has_texcoord && !has_normals)
    {
      switch(n.texture_projection)
      {
        case 4: // Needs just position
          defaultPassesInit(
              renderer,
              mesh,
              shaders.viewspaceVS,
              shaders.viewspaceFS,
              additional_bindings);
          break;
        case 5: // Needs just position
          defaultPassesInit(
              renderer,
              mesh,
              shaders.barycentricVS,
              shaders.barycentricFS,
              additional_bindings);
          break;
        default: // Needs texture coordinates or normals this mesh lacks
          if(has_colors)
            defaultPassesInit(
                renderer, mesh, shaders.colorVS, shaders.colorFS, additional_bindings);
          else
            defaultPassesInit(
                renderer,
                mesh,
                shaders.viewspaceVS,
                shaders.viewspaceFS,
                additional_bindings);
          break;
      }
    }
  }

  void initPasses(RenderList& renderer, const Mesh& mesh)
  {
    auto& n = (ModelDisplayNode&)node;

    createShaders(renderer, mesh);
    m_curShader = n.texture_projection;
    m_draw_mode = n.draw_mode;
    m_camera_mode = n.camera_mode;

    m_blend_color_src = n.blend_color_src;
    m_blend_color_dst = n.blend_color_dst;
    m_blend_color_op = n.blend_color_op;
    m_blend_alpha_src = n.blend_alpha_src;
    m_blend_alpha_dst = n.blend_alpha_dst;
    m_blend_alpha_op = n.blend_alpha_op;
    m_blend_enabled = n.blend_enabled;
    m_prefer_vertex_colors = n.prefer_vertex_colors;

    // Pick triangle- vs point-topology shader set, then index by
    // camera_mode. Values outside [0, CAMERA_MODE_COUNT) clamp to
    // perspective so a stale UI value never indexes out-of-bounds.
    const int mode = (m_camera_mode >= 0 && m_camera_mode < CAMERA_MODE_COUNT)
                         ? m_camera_mode
                         : 0;
    auto& set = (m_draw_mode == 1) ? point_shaders[mode] : triangle_shaders[mode];
    initPasses_impl(renderer, mesh, set);

    // Same for the blend controls: a cable or an OSC message can send any int,
    // and QRhi asserts on (release: Q_UNREACHABLE for) an unknown factor/op.
    using P = QRhiGraphicsPipeline;
    auto factor = [](int v, P::BlendFactor def) {
      return v >= int(P::Zero) && v <= int(P::OneMinusSrc1Alpha) ? P::BlendFactor(v) : def;
    };
    auto op = [](int v) {
      return v >= int(P::Add) && v <= int(P::Max) ? P::BlendOp(v) : P::Add;
    };
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = m_blend_enabled;
    blend.srcColor = factor(m_blend_color_src, P::One);
    blend.dstColor = factor(m_blend_color_dst, P::Zero);
    blend.opColor = op(m_blend_color_op);
    blend.srcAlpha = factor(m_blend_alpha_src, P::One);
    blend.dstAlpha = factor(m_blend_alpha_dst, P::Zero);
    blend.opAlpha = op(m_blend_alpha_op);

    for(auto& [e, pass] : this->m_p)
    {
      pass.p.pipeline->destroy();

      pass.p.pipeline->setTargetBlends({blend});

      switch(m_draw_mode)
      {
        case 0:
          pass.p.pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
          break;
        case 1:
          pass.p.pipeline->setTopology(QRhiGraphicsPipeline::Points);
          break;
        case 2:
          pass.p.pipeline->setTopology(QRhiGraphicsPipeline::Lines);
          break;
      }

      // Reverse-Z project rule (matches PipelineStateHelpers::applyPipelineState
      // default). buildPipeline leaves DepthOp at QRhi's default `Less` which
      // rejects every fragment against the 0.0-cleared reverse-Z buffer.
      pass.p.pipeline->setDepthTest(true);
      pass.p.pipeline->setDepthWrite(true);
      pass.p.pipeline->setDepthOp(QRhiGraphicsPipeline::Greater);

      // Every vertex shader in this file negates gl_Position.y under
      // QSHADER_HLSL / QSHADER_MSL (eight sites, see the Y-convention note
      // above) so an offscreen texture lands top-row-first everywhere. That
      // mirror also flips the WINDOW-SPACE WINDING, which is what the
      // rasteriser classifies front and back faces by -- and QRhi does not
      // normalise winding across backends. RenderedVSANode.cpp:180-199 already
      // records this hazard for the VSA path, where it was solved by disabling
      // culling; a model needs its culling, so the front face is compensated
      // instead.
      //
      // Without this, the SAME declared FrontFace culls OPPOSITE face sets:
      // D3D11 and OpenGL/Vulkan each draw exactly the faces the other
      // discards. On a correctly wound (CCW-out) model that means its NEAR
      // faces are culled on D3D and the viewer sees straight through it.
      //
      // GL and Vulkan pipelines are untouched: their baked shader does not
      // mirror Y, so their winding already matches the declared front face.
      switch(renderer.state.api)
      {
        case score::gfx::D3D11:
        case score::gfx::D3D12:
        case score::gfx::Metal:
          if(pass.p.pipeline->cullMode() != QRhiGraphicsPipeline::None)
            pass.p.pipeline->setFrontFace(
                pass.p.pipeline->frontFace() == QRhiGraphicsPipeline::CCW
                    ? QRhiGraphicsPipeline::CW
                    : QRhiGraphicsPipeline::CCW);
          break;
        default:
          break;
      }

      pass.p.pipeline->create();
    }
  }

  QString processVertexShader(
      QString init, std::string_view out, std::string_view proc, std::string_view camera,
      const score::gfx::Mesh& mesh)
  {

    std::string vtx_define_filters;
    std::string vtx_do_filters;
    // Add additional bindings.
    // 0: renderer
    // 1: processUBO
    // 2: materialUBO
    // 3: input texture
    // 4: it starts here :)
    int cur_binding = 4;

    if (mesh.filters)
    {
      if (!mesh.filters->filters.empty())
      {
        for (auto& f : mesh.filters->filters)
        {
          auto shader = f.shader;
          boost::algorithm::replace_first(
              shader, "%next%", std::to_string(cur_binding++));
          vtx_define_filters += shader;
          vtx_do_filters += fmt::format(
              "process_vertex_{}(in_position, in_normal, in_uv, in_tangent, "
              "in_color);",
              f.filter_id);
        }
      }
    }

    init.replace("%vtx_define_filters%", vtx_define_filters.data());
    init.replace("%vtx_do_filters%", vtx_do_filters.data());
    init.replace("%vtx_do_projection%", camera.data());
    init.replace("%vtx_output%", out.data());
    init.replace("%vtx_output_process%", proc.data());
    return init;
  }

  void createShaders(
      RenderShaders& target, RenderList& renderer, std::string_view vtx_output,
      std::string_view vtx_output_process, std::string_view camera,
      const score::gfx::Mesh& mesh)
  {
    const QString triangle_phongVS = processVertexShader(
        model_display_vertex_shader_phong, vtx_output, vtx_output_process, camera, mesh);
    const QString triangle_texcoordVS = processVertexShader(
        model_display_vertex_shader_texcoord, vtx_output, vtx_output_process, camera,
        mesh);
    const QString triangle_triplanarVS = processVertexShader(
        model_display_vertex_shader_triplanar, vtx_output, vtx_output_process, camera,
        mesh);
    const QString triangle_sphericalVS = processVertexShader(
        model_display_vertex_shader_spherical, vtx_output, vtx_output_process, camera,
        mesh);
    const QString triangle_spherical2VS = processVertexShader(
        model_display_vertex_shader_spherical2, vtx_output, vtx_output_process, camera,
        mesh);
    const QString triangle_viewspaceVS = processVertexShader(
        model_display_vertex_shader_viewspace, vtx_output, vtx_output_process, camera,
        mesh);
    const QString triangle_barycentricVS = processVertexShader(
        model_display_vertex_shader_barycentric, vtx_output, vtx_output_process, camera,
        mesh);
    const QString triangle_colorVS = processVertexShader(
        model_display_vertex_shader_color, vtx_output, vtx_output_process, camera, mesh);

    std::tie(target.phongVS, target.phongFS) = score::gfx::makeShaders(
        renderer.state, triangle_phongVS, model_display_fragment_shader_phong);
    std::tie(target.texCoordVS, target.texCoordFS) = score::gfx::makeShaders(
        renderer.state, triangle_texcoordVS, model_display_fragment_shader_texcoord);
    std::tie(target.triplanarVS, target.triplanarFS) = score::gfx::makeShaders(
        renderer.state, triangle_triplanarVS, model_display_fragment_shader_triplanar);
    std::tie(target.sphericalVS, target.sphericalFS) = score::gfx::makeShaders(
        renderer.state, triangle_sphericalVS, model_display_fragment_shader_spherical);
    std::tie(target.spherical2VS, target.spherical2FS) = score::gfx::makeShaders(
        renderer.state, triangle_spherical2VS, model_display_fragment_shader_spherical2);
    std::tie(target.viewspaceVS, target.viewspaceFS) = score::gfx::makeShaders(
        renderer.state, triangle_viewspaceVS, model_display_fragment_shader_viewspace);
    std::tie(target.barycentricVS, target.barycentricFS) = score::gfx::makeShaders(
        renderer.state, triangle_barycentricVS,
        model_display_fragment_shader_barycentric);
    std::tie(target.colorVS, target.colorFS) = score::gfx::makeShaders(
        renderer.state, triangle_colorVS, model_display_fragment_shader_color);
  }

  void createShaders(RenderList& renderer, const score::gfx::Mesh& mesh)
  {
    // One projection snippet per camera_mode — order MUST match the UI
    // enum ordering described on RenderShaders.
    const char* projections[CAMERA_MODE_COUNT] = {
        vtx_projection_perspective,
        vtx_projection_fulldome_equidistant,
        vtx_projection_fulldome_equisolid,
        vtx_projection_fulldome_stereographic,
        vtx_projection_fulldome_orthographic,
    };

    for(int i = 0; i < CAMERA_MODE_COUNT; ++i)
    {
      createShaders(
          triangle_shaders[i], renderer, vtx_output_triangle,
          vtx_output_process_triangle, projections[i], mesh);
      createShaders(
          point_shaders[i], renderer, vtx_output_point, vtx_output_process_point,
          projections[i], mesh);
    }
  }

  void recreateRenderTarget(RenderList& renderer)
  {
    auto& node = static_cast<const ModelDisplayNode&>(this->node);
    auto& rhi = *renderer.state.rhi;

    SCORE_ASSERT(m_samplers.empty());

    m_renderer = &renderer;

    // Sampler for input texture
    auto rt_spec = node.resolveRenderTargetSpecs(0, renderer);

    auto sampler = rhi.newSampler(
        rt_spec.min_filter, rt_spec.mag_filter, QRhiSampler::Linear, rt_spec.address_u,
        rt_spec.address_v, rt_spec.address_w);
    sampler->setName("ModelDisplayNode::init::sampler");
    SCORE_ASSERT(sampler->create());

    m_samplers.push_back({sampler, inputTexture(renderer)});
  }

  // With no cable, the texture inlet's render target is never drawn into and
  // reads black, which hides the mesh in every projection mode: sample white
  // instead, so an untextured model shows its shape.
  QRhiTexture* inputTexture(RenderList& renderer) noexcept
  {
    auto& in = *this->node.input[0];
    if(in.edges.empty())
    {
      const int pattern = static_cast<const ModelDisplayNode&>(this->node).fallback_pattern;
      if(auto* tex = patternTexture(renderer, pattern))
        return tex;
      return &renderer.whiteTexture();
    }
    auto inputRT = renderer.renderTargetForInputPort(in);
    return inputRT.texture ? inputRT.texture : &renderer.emptyTexture();
  }

  //! A checker of 8 by 8 squares, and a texture lit near u = 0, v = 0 and
  //! u + v = 1: sampled at barycentric coordinates, the edges of triangles.
  //! Drawn once for the process.
  static const QImage& patternImage(int pattern)
  {
    static const std::array<QImage, 3> images = [] {
      constexpr int size = 256;
      std::array<QImage, 3> res;
      for(int p = 1; p < 3; p++)
      {
        QImage img(size, size, QImage::Format_RGBA8888);
        for(int y = 0; y < size; y++)
        {
          auto* line = reinterpret_cast<uint8_t*>(img.scanLine(y));
          for(int x = 0; x < size; x++)
          {
            std::array<uint8_t, 4> c;
            if(p == 1)
            {
              const bool odd = ((x * 8 / size) + (y * 8 / size)) % 2;
              c = odd ? std::array<uint8_t, 4>{235, 235, 235, 255}
                      : std::array<uint8_t, 4>{70, 120, 200, 255};
            }
            else
            {
              const double u = (x + 0.5) / size, v = (y + 0.5) / size;
              const double edge = std::min({u, v, std::abs(1. - u - v)});
              c = edge < 0.04 ? std::array<uint8_t, 4>{255, 255, 255, 255}
                              : std::array<uint8_t, 4>{25, 30, 40, 255};
            }
            std::copy(c.begin(), c.end(), line + 4 * x);
          }
        }
        res[p] = std::move(img);
      }
      return res;
    }();
    return images[pattern];
  }

  //! The texture of a fallback pattern, created the first time a renderer
  //! shows it and uploaded with the next update
  QRhiTexture* patternTexture(RenderList& renderer, int pattern)
  {
    if(pattern <= 0 || pattern >= int(m_patterns.size()))
      return nullptr;
    if(!m_patterns[pattern])
    {
      // Mipmapped: the input sampler filters between mip levels
      auto* tex = renderer.state.rhi->newTexture(
          QRhiTexture::RGBA8, patternImage(pattern).size(), 1,
          QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips);
      tex->setName("ModelDisplayNode::pattern");
      if(!tex->create())
      {
        delete tex;
        return nullptr;
      }
      m_patterns[pattern] = tex;
      m_patternPending[pattern] = true;
    }
    return m_patterns[pattern];
  }

  void initState(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    recreateRenderTarget(renderer);
    const auto& mesh = m_mesh ? *m_mesh : renderer.defaultQuad();

    defaultMeshInit(renderer, mesh, res);
    processUBOInit(renderer);
    m_material.init(renderer, node.input, m_samplers);

    m_initialized = true;
  }

  void addOutputPass(
      RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res) override
  {
    // An edge connected after the initial build needs the full rebuild too, or
    // it renders nothing until an unrelated material/geometry change happens to
    // retrigger mustRecreatePasses.
    if(!m_p.empty() && hasOutputPassForEdge(edge))
      return;
    for(auto& pass : m_p)
      pass.second.release();
    m_p.clear();

    const auto& mesh = m_mesh ? *m_mesh : renderer.defaultQuad();
    initPasses(renderer, mesh);
  }

  bool hasOutputPassForEdge(Edge& edge) const override
  {
    return ossia::find_if(m_p, [&](const auto& p) { return p.first == &edge; })
           != m_p.end();
  }

  void releaseState(RenderList& r) override
  {
    if(!m_initialized)
      return;

    m_renderer = nullptr;

    if(m_scene)
      m_scene->release();
    releaseGuides();
    releaseBounds(r);
    for(auto& pass : m_p)
      pass.second.release();
    m_p.clear();

    for(auto sampler : m_samplers)
    {
      delete sampler.sampler;
    }
    m_samplers.clear();
    for(auto*& tex : m_patterns)
    {
      if(tex)
        tex->deleteLater();
      tex = nullptr;
    }
    m_patternPending = {};

    delete m_processUBO;
    m_processUBO = nullptr;

    delete m_material.buffer;
    m_material.buffer = nullptr;

    m_meshbufs = {};

    m_initialized = false;
  }

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    initState(renderer, res);

    const auto& mesh = m_mesh ? *m_mesh : renderer.defaultQuad();
    initPasses(renderer, mesh);
  }

  template <std::size_t N>
  static void fromGL(const float (&from)[N], auto& to)
  {
    memcpy(to.data(), from, sizeof(float[N]));
  }
  template <std::size_t N>
  static void fromGL(float (&from)[N], auto& to)
  {
    memcpy(to.data(), from, sizeof(float[N]));
  }
  template <std::size_t N>
  static void toGL(auto& from, float (&to)[N])
  {
    memcpy(to, from.data(), sizeof(float[N]));
  }

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override
  {
    auto& n = static_cast<const ModelDisplayNode&>(this->node);
    // Before the camera, which may frame the geometry
    {
      auto& model = const_cast<ModelDisplayNode&>(n);
      collectBounds(model);
      const bool sceneChanged = m_scene && m_scene->setScene(this->scene);
      if(this->geometryChanged || m_fitRequest != n.fit_request
         || (sceneChanged && drawsScene()))
      {
        m_fitRequest = n.fit_request;
        measure(model);
      }
    }

    // A cable added or removed on the texture inlet does not always
    // re-initialise this renderer.
    if(!m_samplers.empty())
      if(auto* tex = inputTexture(renderer); m_samplers[0].texture != tex)
        GenericNodeRenderer::updateInputTexture(*n.input[0], tex);

    // After the lookup above, which may create one
    for(std::size_t i = 0; i < m_patterns.size(); i++)
      if(m_patterns[i] && m_patternPending[i])
      {
        res.uploadTexture(m_patterns[i], patternImage(int(i)));
        res.generateMips(m_patterns[i]);
        m_patternPending[i] = false;
      }

    bool mustRecreatePasses = false;
    // A resize of the output changes the aspect ratio without re-initialising
    // this renderer when its texture inlet has a size of its own.
    if(this->materialChanged || m_projectionSize != renderer.state.renderSize)
    {
      m_projectionSize = renderer.state.renderSize;
      QMatrix4x4 model{};
      fromGL(n.ubo.model, model);

      QMatrix4x4 projection;
      // FIXME should use the size of the target instead
      // Since we can render on multiple target, this means that we must have one UBO for each
      projection.perspective(
          n.fov,
          qreal(renderer.state.renderSize.width()) / renderer.state.renderSize.height(),
          n.near,
          n.far);

      // Project-wide reverse-Z convention: near=1, far=0, depth cleared to
      // 0.0, depth op Greater. QMatrix4x4::perspective() produces standard
      // GL Z (near=-1, far=+1) which clipSpaceCorrMatrix then maps to
      // Vulkan [0, 1] — the wrong direction for reverse-Z.
      //
      // Pre-multiplying by a Z-flip matrix flips the NDC z output of the
      // perspective: z_ndc → -z_ndc. After clipSpaceCorrMatrix's [-1,1] →
      // [0,1] remap, that gives near→1, far→0, exactly what the rest of
      // the pipeline expects.
      {
        QMatrix4x4 zFlip;
        zFlip(2, 2) = -1.0f;
        projection = zFlip * projection;
      }
      QMatrix4x4 view;

      view.lookAt(
          QVector3D{n.position[0], n.position[1], n.position[2]},
          QVector3D{n.center[0], n.center[1], n.center[2]},
          QVector3D{n.up[0], n.up[1], n.up[2]});
      m_view.model = model;
      m_view.view = view;
      m_view.eye = QVector3D{n.position[0], n.position[1], n.position[2]};
      m_view.changed = true;

      QMatrix4x4 mv = view * model;
      QMatrix4x4 mvp = projection * view * model;
      QMatrix3x3 norm = model.normalMatrix();

      ModelCameraUBO mc;
      std::fill_n((char*)&mc, sizeof(ModelCameraUBO), 0);
      toGL(model, mc.model);
      toGL(projection, mc.projection);
      toGL(view, mc.view);
      toGL(mv, mc.mv);
      toGL(mvp, mc.mvp);
      // std140 mat3 = three vec4-aligned columns. modelNormal is 12
      // floats; toGL would memcpy 48 bytes from the 36-byte QMatrix3x3
      // (OOB read + garbled columns). Spread the 9 values by column.
      {
        const float* nd = norm.constData();
        for(int c = 0; c < 3; c++)
          for(int r = 0; r < 3; r++)
            mc.modelNormal[c * 4 + r] = nd[c * 3 + r];
      }
      mc.fov = n.fov;
      mc.znear = n.near;
      mc.zfar = n.far;

      res.updateDynamicBuffer(m_material.buffer, 0, sizeof(ModelCameraUBO), &mc);

      if(m_curShader != n.texture_projection)
        mustRecreatePasses = true;
      if (m_draw_mode != n.draw_mode)
        mustRecreatePasses = true;
      if(m_camera_mode != n.camera_mode)
        mustRecreatePasses = true;
      if(m_blend_color_src != n.blend_color_src)
        mustRecreatePasses = true;
      if(m_blend_color_dst != n.blend_color_dst)
        mustRecreatePasses = true;
      if(m_blend_color_op != n.blend_color_op)
        mustRecreatePasses = true;
      if(m_blend_alpha_src != n.blend_alpha_src)
        mustRecreatePasses = true;
      if(m_blend_alpha_dst != n.blend_alpha_dst)
        mustRecreatePasses = true;
      if(m_blend_alpha_op != n.blend_alpha_op)
        mustRecreatePasses = true;
      if(m_blend_enabled != n.blend_enabled)
        mustRecreatePasses = true;
      if(m_prefer_vertex_colors != n.prefer_vertex_colors)
        mustRecreatePasses = true;
    }
    this->materialChanged = false;

    res.updateDynamicBuffer(m_processUBO, 0, sizeof(ProcessUBO), &n.standardUBO);


    if(this->geometryChanged)
    {
      auto old_meshes = m_mesh;
      auto old_bufs = m_meshbufs;
      if(geometry.meshes)
      {
        std::tie(m_mesh, m_meshbufs)
            = renderer.acquireMesh(geometry, res, m_mesh, m_meshbufs);
        SCORE_ASSERT(m_mesh);

        this->meshChangedIndex = this->m_mesh->dirtyGeometryIndex;
      }
      if(old_meshes != m_mesh || old_bufs.buffers != m_meshbufs.buffers)
        mustRecreatePasses = true;
      this->geometryChanged = false;
    }

    const auto& mesh = m_mesh ? *m_mesh : renderer.defaultQuad();
    // The geometry input went away (removeInputEdge): draw the default quad,
    // whose buffers init() would otherwise have set up.
    if(!m_mesh && m_meshbufs.buffers.empty())
    {
      m_meshbufs = renderer.initMeshBuffer(mesh, res);
      mustRecreatePasses = true;
    }
    // New contents in the same buffers
    if(m_mesh && m_mesh->dirtyGeometryIndex != m_boundsMeshIndex)
    {
      m_boundsMeshIndex = m_mesh->dirtyGeometryIndex;
      measure(const_cast<ModelDisplayNode&>(n));
    }
    // FIXME is that neeeded?
    if(mesh.hasGeometryChanged(meshChangedIndex)) {
      mustRecreatePasses = true;
    }
    // A filter UBO baked into the SRBs went away or was recreated.
    if(m_inputRemoved || !filtersStillBound(renderer, mesh))
      mustRecreatePasses = true;
    m_inputRemoved = false;

    if (mustRecreatePasses)
    {
      for (auto& pass : m_p)
        pass.second.release();
      m_p.clear();

      initPasses(renderer, mesh);
    }

    if(m_renderer)
      if(auto inputRT = m_renderer->renderTargetForInputPort(*this->node.input[0]);
         inputRT.texture
         && inputRT.texture->flags().testFlag(QRhiTexture::UsedWithGenerateMips))
        // Only the texture's OWNER knows whether it was allocated with mip
        // support; an input render target produced by an upstream node
        // usually was not. qrhivulkan asserts on generateMips() for a
        // texture created without UsedWithGenerateMips (GL silently
        // tolerates it), which aborts the whole model pipeline in a debug
        // Qt Vulkan build. Sampling mip 0 is correct when no mip chain
        // exists, so skip the request rather than abort.
        score::gfx::generateMipsIfAny(res, inputRT.texture);

    if(m_scene)
    {
      m_scene->update(renderer, res, m_p, m_processUBO, m_material.buffer, m_view);
      m_view.changed = false;
    }
    updateGuides(renderer, res);
  }

  void runRenderPass(RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge) override
  {
    const auto& mesh = m_mesh ? *m_mesh : renderer.defaultQuad();
    drawGuides(renderer, cb, edge, true);
    if(drawsScene())
      m_scene->draw(renderer, cb, edge);
    else
      defaultRenderPass(renderer, mesh, cb, edge);
    drawGuides(renderer, cb, edge, false);
    drawSceneLines(renderer, cb, edge);
  }

  //! The gizmos of the scene, with the line pipeline of the guides
  void drawSceneLines(RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
  {
    if(!drawsScene() || !m_guideVertices)
      return;
    auto gp = ossia::find_if(m_guidePasses, [&](const auto& g) { return g.edge == &edge; });
    if(gp != m_guidePasses.end())
      m_scene->drawLines(renderer, cb, edge, gp->lines, gp->srb);
  }

  void removeInputEdge(RenderList& r, Edge& edge) override
  {
    score::gfx::NodeRenderer::removeInputEdge(r, edge);
    if(!edge.sink || edge.sink->type != score::gfx::Types::Geometry)
      return;
    // The upstream geometry (and the filters it carries, whose UBOs our SRBs
    // reference) is going away with the edge: forget the mesh, and rebuild the
    // passes on the next update, once the upstream renderers are gone from
    // the list.
    geometry = {};
    scene = {};
    m_mesh = nullptr;
    m_meshbufs = {};
    this->geometryChanged = true;
    m_inputRemoved = true;
  }

  void release(RenderList& r) override
  {
    m_renderer = nullptr;
    // m_mesh belongs to the RenderList going away: acquireMesh() hands out a
    // pointer into its custom-mesh cache, which RenderList::release() deletes
    // right after this. Keeping it leaves a dangling Mesh for the next init()
    // to pass to initMeshBuffer() -- a resize or a full-screen change away,
    // since both rebuild the list mid-frame through maybeRebuild().
    // Dropping it makes init() fall back to the default quad; geometryChanged
    // brings the real mesh back on the next update against the new list.
    m_mesh = nullptr;
    m_meshbufs = {};
    this->geometryChanged = true;
    if(m_scene)
      m_scene->release();
    releaseGuides();
    releaseBounds(r);
    defaultRelease(r);
  }
};

NodeRenderer* ModelDisplayNode::createRenderer(RenderList& r) const noexcept
{
  auto* renderer = new Renderer{*this};
  renderer->m_scene = createSceneDrawer();
  return renderer;
}

std::unique_ptr<ModelSceneDrawer> ModelDisplayNode::createSceneDrawer() const
{
  return nullptr;
}

void ModelDisplayNode::processControl(int32_t p, const ossia::value& v)
{
  const ossia::value* val = &v;
  switch(p)
  {
    case 2: {
      this->position = ossia::convert<ossia::vec3f>(*val);
      this->materialChange();
      break;
    }
    case 3: {
      this->center = ossia::convert<ossia::vec3f>(*val);
      this->materialChange();
      break;
    }
    case 4: {
      this->fov = ossia::convert<float>(*val);
      this->materialChange();
      break;
    }
    case 5: {
      this->near = ossia::convert<float>(*val);
      this->materialChange();
      break;
    }
    case 6: {
      this->far = ossia::convert<float>(*val);
      this->materialChange();
      break;
    }
    case 7: {
      this->texture_projection = ossia::convert<int>(*val);
      this->materialChange();
      break;
    }
    case 8: {
      this->draw_mode = ossia::convert<int>(*val);
      this->materialChange();
      break;
    }
    case 9: {
      this->camera_mode = ossia::convert<int>(*val);
      this->materialChange();
      break;
    }
    case 10: {
      this->blend_enabled = ossia::convert<bool>(*val);
      this->materialChange();
      break;
    }
    case 11: {
      this->blend_color_src = ossia::convert<int>(*val);
      this->materialChange();
      break;
    }
    case 12: {
      this->blend_color_dst = ossia::convert<int>(*val);
      this->materialChange();
      break;
    }
    case 13: {
      this->blend_color_op = ossia::convert<int>(*val);
      this->materialChange();
      break;
    }
    case 14: {
      this->blend_alpha_src = ossia::convert<int>(*val);
      this->materialChange();
      break;
    }
    case 15: {
      this->blend_alpha_dst = ossia::convert<int>(*val);
      this->materialChange();
      break;
    }
    case 16: {
      this->blend_alpha_op = ossia::convert<int>(*val);
      this->materialChange();
      break;
    }
  }
}

void ModelDisplayNode::process(Message&& msg)
{
  ProcessNode::process(msg.token);

  int32_t p = 0;
  for (const gfx_input& m : msg.input)
  {
    if (auto val = ossia::get_if<ossia::value>(&m))
    {
      processControl(p, *val);
      p++;
    }
    else if(auto val = ossia::get_if<ossia::render_target_spec>(&m))
    {
      ProcessNode::process(p, *val);

      p++;
    }
    else
    {
      p++;
    }
  }
}

void ModelDisplayNode::process(int32_t port, const ossia::transform3d& val)
{
  memcpy(this->ubo.model, val.matrix, sizeof(val.matrix));
  this->materialChange();
}
}
