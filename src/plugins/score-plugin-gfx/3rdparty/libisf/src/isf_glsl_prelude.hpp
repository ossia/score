#pragma once
// The GLSL text libisf puts around every shader it generates: the #version
// line, the vertex / fragment preludes, the renderer and process uniform
// blocks, the image and storage macros (IMG_PIXEL, IMG_NORM_PIXEL, IMG_LOAD,
// ISF_READ / ISF_WRITE...) and the helper functions. Kept apart from the
// parser so the generated code reads as one place (shader board U31).
// Included by isf.cpp only.

namespace isf
{
namespace
{
static constexpr struct glsl45_t
{
  // Identifiers the target languages reserve (quad / operator / new on Metal,
  // lerp / frac on D3D...) are renamed per target at bake time, for every
  // shader: score::gfx::renameReservedIdentifiers in ShaderCache.
  static constexpr auto versionPrelude = R"_(#version 460
)_";

  static constexpr auto vertexPrelude = R"_(
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec2 isf_FragNormCoord;

)_";
  static constexpr auto vertexInitFunc = R"_(
void isf_vertShaderInit()
{
  gl_Position = clipSpaceCorrMatrix * vec4(position, 0.0, 1.0);
  isf_FragNormCoord = vec2((gl_Position.x+1.0)/2.0, (gl_Position.y+1.0)/2.0);
}

// The one clip-space Y negation for the whole vertex stage. It has to run
// last, after any transform the shader applies, and it must not sit in
// isf_vertShaderInit: a shader that reads gl_Position back after calling init
// would then see the opposite polarity on Vulkan. Every ISF vertex shader ends
// its main with a call to this.
void isf_vertShaderFinish()
{
#if defined(QSHADER_SPIRV) || defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = -gl_Position.y;
#endif
}
)_";

  static constexpr auto vertexDefaultMain = R"_(
void main()
{
  isf_vertShaderInit();
  isf_vertShaderFinish();
}
)_";

  static constexpr auto fragmentPrelude = R"_(
layout(location = 0) in vec2 isf_FragNormCoord;
layout(location = 0) out vec4 isf_FragColor;
)_";

  static constexpr auto defaultUniforms = R"_(
// Shared uniform buffer for the whole render window
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix_;

  vec2 RENDERSIZE_;
  // MSAA sample count of the active output target (1 when MSAA is off).
  // Mirrors RenderList::samples(); needed because glslang strips
  // gl_NumSamples under SPIR-V. _pad0 keeps the struct vec4-aligned.
  int MSAA_SAMPLES_;
  int _renderer_pad0_;
} isf_renderer_uniforms;

// This dance is needed because otherwise
// spirv-cross may generate different struct names in the vertex & fragment, causing crashes..
// but we have to keep compat with ISF
#define clipSpaceCorrMatrix isf_renderer_uniforms.clipSpaceCorrMatrix_
#define MSAA_SAMPLES isf_renderer_uniforms.MSAA_SAMPLES_

// Time-dependent uniforms, only relevant during execution
layout(std140, binding = 1) uniform process_t {
  float TIME_;
  float TIMEDELTA_;
  float PROGRESS_;
  float SAMPLERATE_;

  int PASSINDEX_;
  int FRAMEINDEX_;

  vec2 RENDERSIZE_;
  vec4 DATE_;
  // Mirrors gl_NumWorkGroups for compute shaders. SPIRV-Cross's HLSL
  // backend refuses to emit code for the NumWorkgroups built-in unless
  // remap_num_workgroups_builtin() is set up on both the cross-compiler
  // and the QRhi side; QShaderBaker exposes neither, so any compute
  // shader using gl_NumWorkGroups silently fails to bake to HLSL on
  // D3D11/D3D12. We sidestep that by routing references through this
  // uniform — populated host-side just before each dispatch — and
  // textually shadowing the built-in via the #define below.
  uvec3 NUMWORKGROUPS_;
  // The view a multiview shader renders where the backend cannot amplify
  // (MULTIVIEW lowered to one pass per view, see ShaderCache): gl_ViewIndex
  // reads this. Kept apart from PASSINDEX, which PER_MIP already spends on
  // the mip level.
  int VIEWINDEX_;
} isf_process_uniforms;

#define TIME isf_process_uniforms.TIME_
#define TIMEDELTA isf_process_uniforms.TIMEDELTA_
#define PROGRESS isf_process_uniforms.PROGRESS_
#define PASSINDEX isf_process_uniforms.PASSINDEX_
#define FRAMEINDEX isf_process_uniforms.FRAMEINDEX_
#define RENDERSIZE isf_process_uniforms.RENDERSIZE_
#define DATE isf_process_uniforms.DATE_
#define SAMPLERATE isf_process_uniforms.SAMPLERATE_
#define gl_NumWorkGroups isf_process_uniforms.NUMWORKGROUPS_
#define isf_NumWorkGroups isf_process_uniforms.NUMWORKGROUPS_
)_";

  // Storage-image access for compute shaders, in the author's coordinate system.
  //
  // imageStore() addresses a raw texel index, where row 0 is first in memory on
  // every backend, while the render target the result is copied into is
  // bottom-up on OpenGL and top-down everywhere else -- what
  // QRhi::isYUpInFramebuffer() reports. The two spaces are therefore mirrored
  // against each other on OpenGL, and a compute shader that only stores comes
  // out upside down there.
  //
  // The gate is the inverse of the fragment macros above: a fragment shader
  // needs its correction on Vulkan, a compute shader needs it everywhere else.
  // Read and store are corrected TOGETHER, so a shader that samples an input and
  // stores it at the matching index keeps agreeing with itself.
  //
  // A cube face is addressed by direction, so no Y convention applies to it.
  static constexpr auto computeImageMacros =
      R"_(
// Everything except Vulkan, measured rather than derived. The framebuffer
// origin does not predict this: Direct3D and Metal put it at the top like
// Vulkan, yet a compute storage image reaches the delivered picture mirrored on
// them exactly as it does on OpenGL. Gating this on OpenGL alone leaves D3D11,
// D3D12 and Metal upside down -- csf_orient_macros reports green=255 where 0
// is expected at row 0 on all three.
#if defined(QSHADER_SPIRV)
#define ISF_STORE_COORD(img, coord) ivec2(coord)
#define ISF_STORE_COORD_LAYER(img, coord) ivec3(coord)
#define ISF_FIXUP_COMPUTE_TEXCOORD(coord) (coord)
#define IMG_TEXEL_PREMULTIPLIED(tex, coord) texelFetch(tex, ivec2(coord), 0)
#else
#define ISF_STORE_COORD(img, coord) ivec2((coord).x, imageSize(img).y - 1 - (coord).y)
#define ISF_STORE_COORD_LAYER(img, coord) ivec3((coord).x, imageSize(img).y - 1 - (coord).y, (coord).z)
#define ISF_FIXUP_COMPUTE_TEXCOORD(coord) vec2((coord).x, 1. - (coord).y)
#define IMG_TEXEL_PREMULTIPLIED(tex, coord) texelFetch(tex, ivec2(ivec2(coord).x, textureSize(tex, 0).y - 1 - ivec2(coord).y), 0)
#endif

#define IMG_STORE(img, coord, val) imageStore(img, ISF_STORE_COORD(img, coord), val)
#define IMG_STORE_LAYER(img, coord, val) imageStore(img, ISF_STORE_COORD_LAYER(img, coord), val)
#define IMG_LOAD(img, coord) imageLoad(img, ISF_STORE_COORD(img, coord))
#define IMG_STORE_CUBE(img, coord, val) imageStore(img, ivec3(coord), val)
#define IMG_LOAD_CUBE(img, coord) imageLoad(img, ivec3(coord))

// Face size of a writable cubemap, as ivec2, on every backend.
//
// A cube storage image is declared `image2DArray` when the target is HLSL --
// Direct3D has no writable cube texture type, so `imageCube` cannot be
// translated at all (isf_emit_cube_image_decl). imageStore / imageLoad take the
// same ivec3 (x, y, face) coordinate for both spellings, so nothing else about
// the author's source changes; imageSize() is the single exception, returning
// ivec2 for a cube and ivec3 for a 2D array. Take the size through this macro
// (or write `imageSize(img).xy`, which is what it expands to) and the shader is
// identical on Direct3D and everywhere else.
#define IMG_SIZE_CUBE(img) (imageSize(img).xy)

vec4 isf_unpremultiply(vec4 c) { return c.a > 0.0 ? vec4(c.rgb / c.a, c.a) : vec4(0.0); }
#define IMG_NORM_PIXEL_PREMULTIPLIED(tex, coord) texture(tex, ISF_FIXUP_COMPUTE_TEXCOORD(coord))
#define IMG_PIXEL_PREMULTIPLIED(tex, coord) texture(tex, ISF_FIXUP_COMPUTE_TEXCOORD((coord) / vec2(textureSize(tex, 0))))
#define IMG_NORM_PIXEL(tex, coord) IMG_NORM_PIXEL_PREMULTIPLIED(tex, coord)
#define IMG_PIXEL(tex, coord) IMG_PIXEL_PREMULTIPLIED(tex, coord)
#define ISF_STRAIGHT_NORM_PIXEL(tex, coord) isf_unpremultiply(IMG_NORM_PIXEL_PREMULTIPLIED(tex, coord))
#define ISF_STRAIGHT_PIXEL(tex, coord) isf_unpremultiply(IMG_PIXEL_PREMULTIPLIED(tex, coord))
#define IMG_TEXEL(tex, coord) IMG_TEXEL_PREMULTIPLIED(tex, coord)
#define ISF_STRAIGHT_TEXEL(tex, coord) isf_unpremultiply(IMG_TEXEL_PREMULTIPLIED(tex, coord))
#define IMG_CUBE(tex, dir) texture(tex, dir)
)_";

  static constexpr auto defaultFunctions =
      R"_(
// GLSL's textureSize is overloaded by sampler dimensionality — sampler2D
// returns ivec2, sampler3D returns ivec3. Authors typically reach for
// TEX_DIMENSIONS regardless of 2D/3D; the *_2D / *_3D aliases below make
// the intended dimensionality explicit in shader source.
#define TEX_DIMENSIONS(tex) textureSize(tex, 0)
#define TEX_DIMENSIONS_2D(tex) textureSize(tex, 0)
#define TEX_DIMENSIONS_3D(tex) textureSize(tex, 0)
#define IMG_SIZE(tex) textureSize(tex, 0)
#define IMG_SIZE_3D(tex) textureSize(tex, 0)

// IMG_CUBE(tex, dir) — canonical colour-cube read; same in both coord systems
// since a direction vector has no Y-flip. IMG_CUBE_DEPTH(tex, dir) —
// canonical depth-cube read for inputs declared DEPTH: true on a cubemap,
// hides the internal `_depth` companion binding.
#define IMG_CUBE(tex, dir) texture(tex, dir)
#define IMG_CUBE_DEPTH(tex, dir) textureLod(tex##_depth, dir, 0.0).r

vec4 isf_unpremultiply(vec4 c) { return c.a > 0.0 ? vec4(c.rgb / c.a, c.a) : vec4(0.0); }

// Pixel-unit reads are in the sampled image's own pixels, as in the ISF spec:
// IMG_PIXEL(image, coord) is coord / size(image), IMG_THIS_PIXEL(image) is
// IMG_PIXEL(image, gl_FragCoord.xy), pixel-aligned. IMG_THIS_NORM_PIXEL is the
// one that stretches the image over the frame.
#if defined(QSHADER_SPIRV)
#define isf_FragCoord vec4(gl_FragCoord.x, RENDERSIZE.y - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w)
#define ISF_FIXUP_TEXCOORD(coord) vec2((coord).x, 1. - (coord).y)
#define IMG_THIS_PIXEL_PREMULTIPLIED(tex) IMG_PIXEL_PREMULTIPLIED(tex, isf_FragCoord.xy)
#define IMG_THIS_NORM_PIXEL_PREMULTIPLIED(tex) texture(tex, ISF_FIXUP_TEXCOORD(isf_FragNormCoord))
#define IMG_PIXEL_PREMULTIPLIED(tex, coord) texture(tex, ISF_FIXUP_TEXCOORD((coord) / vec2(textureSize(tex, 0))))
#define IMG_NORM_PIXEL_PREMULTIPLIED(tex, coord) texture(tex, ISF_FIXUP_TEXCOORD(coord))
#define IMG_THIS_DEPTH(tex) textureLod(tex##_depth, ISF_FIXUP_TEXCOORD(isf_FragNormCoord), 0.0).r
#define IMG_DEPTH_PIXEL(tex, coord) textureLod(tex##_depth, ISF_FIXUP_TEXCOORD((coord) / vec2(textureSize(tex##_depth, 0))), 0.0).r
#define IMG_DEPTH_NORM_PIXEL(tex, coord) textureLod(tex##_depth, ISF_FIXUP_TEXCOORD(coord), 0.0).r
#define IMG_TEXEL_PREMULTIPLIED(tex, coord) texelFetch(tex, ivec2(ivec2(coord).x, textureSize(tex, 0).y - 1 - ivec2(coord).y), 0)
#define IMG_NORM_PIXEL_LOD_PREMULTIPLIED(tex, coord, lod) textureLod(tex, ISF_FIXUP_TEXCOORD(coord), lod)
#else
#define isf_FragCoord gl_FragCoord
#define IMG_THIS_PIXEL_PREMULTIPLIED(tex) IMG_PIXEL_PREMULTIPLIED(tex, isf_FragCoord.xy)
#define IMG_THIS_NORM_PIXEL_PREMULTIPLIED(tex) texture(tex, isf_FragNormCoord)
#define IMG_PIXEL_PREMULTIPLIED(tex, coord) texture(tex, (coord) / vec2(textureSize(tex, 0)))
#define IMG_NORM_PIXEL_PREMULTIPLIED(tex, coord) texture(tex, coord)
#define IMG_THIS_DEPTH(tex) textureLod(tex##_depth, isf_FragNormCoord, 0.0).r
#define IMG_DEPTH_PIXEL(tex, coord) textureLod(tex##_depth, (coord) / vec2(textureSize(tex##_depth, 0)), 0.0).r
#define IMG_DEPTH_NORM_PIXEL(tex, coord) textureLod(tex##_depth, coord, 0.0).r
#define IMG_TEXEL_PREMULTIPLIED(tex, coord) texelFetch(tex, ivec2(coord), 0)
#define IMG_NORM_PIXEL_LOD_PREMULTIPLIED(tex, coord, lod) textureLod(tex, coord, lod)
#endif

// IMG_NORM_PIXEL with an explicit mip level. Use it (with lod 0.0) for reads
// inside loops whose trip count is a uniform: an implicit-gradient texture()
// there makes fxc (D3D11/D3D12-without-dxc, ps_5_0) try to unroll the loop
// and fail (X3570 + X3511) -- or crash outright on big shaders.
#define IMG_NORM_PIXEL_LOD(tex, coord, lod) IMG_NORM_PIXEL_LOD_PREMULTIPLIED(tex, coord, lod)

#define IMG_THIS_PIXEL(tex) IMG_THIS_PIXEL_PREMULTIPLIED(tex)
#define IMG_THIS_NORM_PIXEL(tex) IMG_THIS_NORM_PIXEL_PREMULTIPLIED(tex)
#define IMG_PIXEL(tex, coord) IMG_PIXEL_PREMULTIPLIED(tex, coord)
#define IMG_NORM_PIXEL(tex, coord) IMG_NORM_PIXEL_PREMULTIPLIED(tex, coord)
#define ISF_STRAIGHT_THIS_PIXEL(tex) isf_unpremultiply(IMG_THIS_PIXEL_PREMULTIPLIED(tex))
#define ISF_STRAIGHT_THIS_NORM_PIXEL(tex) isf_unpremultiply(IMG_THIS_NORM_PIXEL_PREMULTIPLIED(tex))
#define ISF_STRAIGHT_PIXEL(tex, coord) isf_unpremultiply(IMG_PIXEL_PREMULTIPLIED(tex, coord))
#define ISF_STRAIGHT_NORM_PIXEL(tex, coord) isf_unpremultiply(IMG_NORM_PIXEL_PREMULTIPLIED(tex, coord))
#define IMG_TEXEL(tex, coord) IMG_TEXEL_PREMULTIPLIED(tex, coord)
#define ISF_STRAIGHT_TEXEL(tex, coord) isf_unpremultiply(IMG_TEXEL_PREMULTIPLIED(tex, coord))
)_";

} GLSL45;
}
}
