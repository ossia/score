#include "ModelDisplayNode.hpp"

#include <Gfx/Graph/GeometryFilterNodeRenderer.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <ossia/detail/algorithms.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <boost/algorithm/string.hpp>
#include <ossia/detail/fmt.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/gfx/port_index.hpp>
#include <ossia/network/value/value.hpp>
#include <ossia/network/value/value_conversion.hpp>
#include <score/tools/Debug.hpp>
#include <score/tools/SafeCast.hpp>

#include <QPainter>

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
// ----------------------------------------------------------------------------
// Fulldome fisheye projections
//
// All four variants share the same θ/φ derivation and the same reverse-Z
// depth; they differ only in the `r_ndc = f(θ)` mapping. Kept as separate
// vertex-shader snippets (rather than a runtime branch on a uniform) so
// the GPU dispatches branch-free code for the selected projection.
//
//   equidistant   — r = θ / (FOV/2)          (domemaster, uniform angular resolution; default)
//   equisolid     — r = sin(θ/2) / sin(FOV/4) (equal-area; typical of photographic fisheye lenses)
//   stereographic — r = tan(θ/2) / tan(FOV/4) (conformal; "little planet" look)
//   orthographic  — r = sin(θ)   / sin(FOV/2) (parallel-projection sphere; FOV ≤ 180° only)
//
// Points with r_ndc > 1 fall outside the NDC unit square and are hardware-
// clipped, so FOV > 180° works out of the box for equidistant / equisolid /
// stereographic. Orthographic cannot exceed 180° geometrically.
// ----------------------------------------------------------------------------
const constexpr auto vtx_projection_fulldome_equidistant = R"_(
//
// Fulldome / domemaster (equidistant angular fisheye).
//
//   r_2D = theta / (fov/2)           — radial image distance (NDC units)
//   theta = angle from dome forward axis (view-space +Z in this convention)
//   phi   = azimuth around forward axis
//
// Convention kept from the original implementation: the .xzy swizzle re-
// orients world +Z as dome-up, world +Y as dome-forward; the view matrix
// then places the zenith along view-space +Z.
//
// Works for FOV > 180° (e.g. 240°): points with theta > FOV/2 land outside
// the NDC unit square and get hardware-clipped. For point clouds each
// vertex is a single point, so no per-primitive clipping subtleties.
//
// Depth: linear reverse-Z in radial distance. z_gl in [-1,+1] such that
// renderer.clipSpaceCorrMatrix (GL→Vulkan Z remap) yields z_vulkan=1 at
// near, z_vulkan=0 at far. Matches the project-wide reverse-Z convention
// (depth cleared to 0.0, compare op Greater).
//
vec4 v_projected = vec4(0.0, 0.0, 0.0, 1.0);
{
  vec4 viewspace = camera.matrixModelView * vec4(in_position.xzy, 1.0);
  vec3 d = viewspace.xyz;
  float r = length(d);

  const float PI = 3.14159265358979323846264338327;

  if(r > 1e-6)
  {
    float theta = acos(clamp(d.z / r, -1.0, 1.0));
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
  vec4 viewspace = camera.matrixModelView * vec4(in_position.xzy, 1.0);
  vec3 d = viewspace.xyz;
  float r = length(d);

  if(r > 1e-6)
  {
    float theta = acos(clamp(d.z / r, -1.0, 1.0));
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
  vec4 viewspace = camera.matrixModelView * vec4(in_position.xzy, 1.0);
  vec3 d = viewspace.xyz;
  float r = length(d);

  if(r > 1e-6)
  {
    float theta = acos(clamp(d.z / r, -1.0, 1.0));
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
  vec4 viewspace = camera.matrixModelView * vec4(in_position.xzy, 1.0);
  vec3 d = viewspace.xyz;
  float r = length(d);

  if(r > 1e-6)
  {
    float theta = acos(clamp(d.z / r, -1.0, 1.0));
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
  // Match the codebase Y-handling convention used by ImageNode et al.:
  // GL is Y-up framebuffer (no flip), Vulkan's Y flip is baked into
  // QRhi's clipSpaceCorrMatrix, but D3D/Metal share Vulkan's framebuffer
  // origin without sharing its NDC sign convention — so we flip here so
  // the offscreen texture lands top-row-first like the other backends,
  // and the screen compositor (ScaledRenderer) keeps its SPIRV-only UV
  // flip. Without this the model rendered fine on GL/Vulkan but ended
  // up upside-down on D3D11/12.
  gl_Position.y = -gl_Position.y;
#endif

  %vtx_output_process%
}
)_";

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
    vec3 light;
    lightPosition.y = sin(TIME) * 20.;
    lightPosition.z = cos(TIME) * 50.;
    if(lightPosition.w == 0.0)
    {
        light = normalize(lightPosition.xyz);
    }
    else
    {
        light = normalize(lightPosition.xyz - esVertex);
    }
    vec3 view = normalize(-esVertex);
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
  // Match the codebase Y-handling convention used by ImageNode et al.:
  // GL is Y-up framebuffer (no flip), Vulkan's Y flip is baked into
  // QRhi's clipSpaceCorrMatrix, but D3D/Metal share Vulkan's framebuffer
  // origin without sharing its NDC sign convention — so we flip here so
  // the offscreen texture lands top-row-first like the other backends,
  // and the screen compositor (ScaledRenderer) keeps its SPIRV-only UV
  // flip. Without this the model rendered fine on GL/Vulkan but ended
  // up upside-down on D3D11/12.
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
  // Match the codebase Y-handling convention used by ImageNode et al.:
  // GL is Y-up framebuffer (no flip), Vulkan's Y flip is baked into
  // QRhi's clipSpaceCorrMatrix, but D3D/Metal share Vulkan's framebuffer
  // origin without sharing its NDC sign convention — so we flip here so
  // the offscreen texture lands top-row-first like the other backends,
  // and the screen compositor (ScaledRenderer) keeps its SPIRV-only UV
  // flip. Without this the model rendered fine on GL/Vulkan but ended
  // up upside-down on D3D11/12.
  gl_Position.y = -gl_Position.y;
#endif

  %vtx_output_process%
}
)_";

const constexpr auto model_display_fragment_shader_triplanar = R"_(#version 450

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;

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

  vec4 xaxis = texture(y_tex, v_coords.yz * scale);
  vec4 yaxis = texture(y_tex, v_coords.xz * scale);
  vec4 zaxis = texture(y_tex, v_coords.xy * scale);
  vec4 tex = xaxis * blending.x + yaxis * blending.y + zaxis * blending.z;

  fragColor = tex;
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
  // Match the codebase Y-handling convention used by ImageNode et al.:
  // GL is Y-up framebuffer (no flip), Vulkan's Y flip is baked into
  // QRhi's clipSpaceCorrMatrix, but D3D/Metal share Vulkan's framebuffer
  // origin without sharing its NDC sign convention — so we flip here so
  // the offscreen texture lands top-row-first like the other backends,
  // and the screen compositor (ScaledRenderer) keeps its SPIRV-only UV
  // flip. Without this the model rendered fine on GL/Vulkan but ended
  // up upside-down on D3D11/12.
  gl_Position.y = -gl_Position.y;
#endif

  %vtx_output_process%
}
)_";
const constexpr auto model_display_fragment_shader_spherical = R"_(#version 450

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;

layout(location = 0) in vec3 v_e;
layout(location = 1) in vec3 v_n;
layout(location = 0) out vec4 fragColor;

void main ()
{
  vec2 uv = vec2(atan(v_n.z, v_n.x), asin(v_n.y));
  uv = uv * vec2(1. / 2. * 3.14159265358979323846264338327, 1. / 3.14159265358979323846264338327) + 0.5;
  fragColor = texture(y_tex, uv);
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
  // flip. Without this the model rendered fine on GL/Vulkan but ended
  // up upside-down on D3D11/12.
  gl_Position.y = -gl_Position.y;
#endif

  %vtx_output_process%
}
)_";
const constexpr auto model_display_fragment_shader_spherical2 = R"_(#version 450

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;

layout(location = 0) in vec3 v_e;
layout(location = 1) in vec3 v_n;
layout(location = 0) out vec4 fragColor;

void main ()
{
  vec3 r = reflect( v_e, v_n );
  float m = 2. * sqrt( pow( r.x, 2. ) + pow( r.y, 2. ) + pow( r.z + 1., 2. ) );
  vec2 vN = r.xy / m + .5;

  fragColor = texture(y_tex, vN.xy);
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
  // flip. Without this the model rendered fine on GL/Vulkan but ended
  // up upside-down on D3D11/12.
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
  // flip. Without this the model rendered fine on GL/Vulkan but ended
  // up upside-down on D3D11/12.
  gl_Position.y = -gl_Position.y;
#endif

  %vtx_output_process%
}
)_";

const constexpr auto model_display_fragment_shader_barycentric = R"_(#version 450

)_" model_display_default_uniforms R"_(

layout(binding = 3) uniform sampler2D y_tex;

layout(location = 1) in vec2 v_bary;
layout(location = 0) out vec4 fragColor;

void main ()
{
  fragColor = texture(y_tex, v_bary);
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
  // flip. Without this the model rendered fine on GL/Vulkan but ended
  // up upside-down on D3D11/12.
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
  int m_curShader{0};
  int m_draw_mode{0};
  int m_camera_mode{0};
  int m_blend_color_src{0};
  int m_blend_color_dst{0};
  int m_blend_color_op{0};
  int m_blend_alpha_src{0};
  int m_blend_alpha_dst{0};
  int m_blend_alpha_op{0};
  bool m_blend_enabled{false};

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

    if(has_colors && n.texture_projection == 7)
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
      if(has_colors)
      {
        // Geometry has vertex colors but no texcoord/normals - use color shader
        defaultPassesInit(
            renderer, mesh, shaders.colorVS, shaders.colorFS, additional_bindings);
      }
      else
      {
        switch(n.texture_projection)
        {
          default:
          case 0:
          case 1:
          case 2:
          case 3:
          case 6:
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

    // Pick triangle- vs point-topology shader set, then index by
    // camera_mode. Values outside [0, CAMERA_MODE_COUNT) clamp to
    // perspective so a stale UI value never indexes out-of-bounds.
    const int mode = (m_camera_mode >= 0 && m_camera_mode < CAMERA_MODE_COUNT)
                         ? m_camera_mode
                         : 0;
    auto& set = (m_draw_mode == 1) ? point_shaders[mode] : triangle_shaders[mode];
    initPasses_impl(renderer, mesh, set);

    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = m_blend_enabled;
    blend.srcColor = (QRhiGraphicsPipeline::BlendFactor)m_blend_color_src;
    blend.dstColor = (QRhiGraphicsPipeline::BlendFactor)m_blend_color_dst;
    blend.opColor = (QRhiGraphicsPipeline::BlendOp)m_blend_color_op;
    blend.srcAlpha = (QRhiGraphicsPipeline::BlendFactor)m_blend_alpha_src;
    blend.dstAlpha = (QRhiGraphicsPipeline::BlendFactor)m_blend_alpha_dst;
    blend.opAlpha = (QRhiGraphicsPipeline::BlendOp)m_blend_alpha_op;

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
      // ModelDisplay's projection matrix now produces reverse-Z NDC, so we
      // must also flip the compare op.
      pass.p.pipeline->setDepthTest(true);
      pass.p.pipeline->setDepthWrite(true);
      pass.p.pipeline->setDepthOp(QRhiGraphicsPipeline::Greater);

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

    auto inputRT = renderer.renderTargetForInputPort(*this->node.input[0]);
    auto* texture = inputRT.texture ? inputRT.texture : &renderer.emptyTexture();
    m_samplers.push_back({sampler, texture});
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
    // initPasses() creates passes for ALL current edges at once; it only
    // covers edges that exist when it runs. An edge connected after the
    // initial build needs the full rebuild too, or it renders nothing
    // until an unrelated material/geometry change happens to retrigger
    // mustRecreatePasses.
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

    // Release any remaining passes
    for(auto& pass : m_p)
      pass.second.release();
    m_p.clear();

    for(auto sampler : m_samplers)
    {
      delete sampler.sampler;
    }
    m_samplers.clear();

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

  int mdupdate_log = 0;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override
  {
    auto& n = static_cast<const ModelDisplayNode&>(this->node);

    if(mdupdate_log < 3)
    {
      qDebug() << "ModelDisplay::update materialChanged=" << this->materialChanged
               << "geometryChanged=" << this->geometryChanged
               << "fov=" << n.fov
               << "passes=" << m_p.size();
      mdupdate_log++;
    }

    bool mustRecreatePasses = false;
    if(this->materialChanged)
    {
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
          QVector3D{0, 1, 0});
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
    // FIXME is that neeeded?
    // FIXME also not handling geometry_filter dirty geom so far
    if(mesh.hasGeometryChanged(meshChangedIndex)) {
      mustRecreatePasses = true;
    }

    if (mustRecreatePasses)
    {
      for (auto& pass : m_p)
        pass.second.release();
      m_p.clear();

      initPasses(renderer, mesh);
    }

    if(m_renderer)
      if(auto inputRT = m_renderer->renderTargetForInputPort(*this->node.input[0]); inputRT.texture)
        res.generateMips(inputRT.texture);
  }

  void runRenderPass(RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge) override
  {
    const auto& mesh = m_mesh ? *m_mesh : renderer.defaultQuad();
    defaultRenderPass(renderer, mesh, cb, edge);
  }

  void release(RenderList& r) override
  {
    m_renderer = nullptr;
    defaultRelease(r);
  }
};

NodeRenderer* ModelDisplayNode::createRenderer(RenderList& r) const noexcept
{
  return new Renderer{*this};
}

void ModelDisplayNode::process(Message&& msg)
{
  ProcessNode::process(msg.token);

  int32_t p = 0;
  for (const gfx_input& m : msg.input)
  {
    if (auto val = ossia::get_if<ossia::value>(&m))
    {
      switch (p)
      {
        case 2:
        {
          this->position = ossia::convert<ossia::vec3f>(*val);
          this->materialChange();
          break;
        }
        case 3:
        {
          this->center = ossia::convert<ossia::vec3f>(*val);
          this->materialChange();
          break;
        }
        case 4:
        {
          this->fov = ossia::convert<float>(*val);
          this->materialChange();
          break;
        }
        case 5:
        {
          this->near = ossia::convert<float>(*val);
          this->materialChange();
          break;
        }
        case 6:
        {
          this->far = ossia::convert<float>(*val);
          this->materialChange();
          break;
        }
        case 7:
        {
          this->texture_projection = ossia::convert<int>(*val);
          this->materialChange();
          break;
        }
        case 8:
        {
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
