#pragma once
#if SCORE_PLUGIN_GFX

#include <Crousti/GpuUtils.hpp>
#include <Crousti/Metadatas.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Uniforms.hpp>

#include <avnd/binding/ossia/port_run_preprocess.hpp>
#include <avnd/common/for_nth.hpp>
namespace oscr
{
static const constexpr auto generic_texgen_vs = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(binding=3) uniform sampler2D y_tex;
layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
}
)_";

static const constexpr auto generic_texgen_fs = R"_(#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform renderer_t {
mat4 clipSpaceCorrMatrix;
vec2 renderSize;
} renderer;

layout(binding=3) uniform sampler2D y_tex;


void main ()
{
  fragColor = texture(y_tex, v_texcoord);
}
)_";

template <typename Node_T>
struct GfxNode;
template <typename Node_T>
struct GfxRenderer;

}
#endif
