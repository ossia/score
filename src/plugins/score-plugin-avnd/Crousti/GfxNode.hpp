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
template <typename Info>
class ProcessModel;
template <typename Node_T>
struct GfxNode;
template <typename Node_T>
struct GfxRenderer;
}
#endif
