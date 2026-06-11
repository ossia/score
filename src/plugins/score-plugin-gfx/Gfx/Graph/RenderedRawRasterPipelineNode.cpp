#include <Gfx/Graph/CustomMesh.hpp>
#include <Gfx/Graph/ISFVisitors.hpp>
#include <Gfx/Graph/PipelineStateHelpers.hpp>
#include <Gfx/Graph/RenderedISFSamplerUtils.hpp>
#include <Gfx/Graph/RenderedRawRasterPipelineNode.hpp>
#include <Gfx/Graph/SSBO.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/small_vector.hpp>
#include <ossia/math/math_expression.hpp>

#include <boost/algorithm/string/replace.hpp>

#include <cctype>
#include <chrono>

namespace score::gfx
{

static const constexpr auto rrp_blit_vs = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;
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
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = - gl_Position.y;
#endif
}
)_";

static const constexpr auto rrp_blit_fs = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(binding = 3) uniform sampler2D blitTexture;
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main() { fragColor = texture(blitTexture, v_texcoord); }
)_";

RenderedRawRasterPipelineNode::RenderedRawRasterPipelineNode(
    const ISFNode& node) noexcept
    : score::gfx::NodeRenderer{node}
    , n{const_cast<ISFNode&>(node)}
{
}

void RenderedRawRasterPipelineNode::updateInputTexture(const Port& input, QRhiTexture* tex, QRhiTexture* depthTex)
{
  // Find which image-type sampler index this port corresponds to
  int sampler_idx = 0;
  for(auto* p : node.input)
  {
    if(p == &input)
      break;
    if(p->type == Types::Image)
    {
      sampler_idx++;
      if((p->flags & Flag::SamplableDepth) == Flag::SamplableDepth)
        sampler_idx++;
    }
  }

  // Match key for replaceTexture MUST be the sampler that's actually
  // in the SRB binding. allSamplers() (line ~155-170) substitutes
  // m_inputSamplerOverrides[i] for m_inputSamplers[i] when an
  // override is present (per-bucket sampler from ScenePreprocessor).
  // Same fix as commit 7d1afd27b applied to FIX-C — see the long
  // comment there. Without this updateInputTexture silently no-ops on
  // every override-bound entry.
  auto srbKey = [&](int i) -> QRhiSampler* {
    if(i >= 0 && i < (int)m_inputSamplerOverrides.size()
       && m_inputSamplerOverrides[i])
      return m_inputSamplerOverrides[i];
    return m_inputSamplers[i].sampler;
  };

  if(sampler_idx < (int)m_inputSamplers.size())
  {
    auto& sampl = m_inputSamplers[sampler_idx];
    if(sampl.texture != tex)
    {
      sampl.texture = tex;
      auto* key = srbKey(sampler_idx);
      for(auto& [e, pass] : m_passes)
        if(pass.p.srb)
          score::gfx::replaceTexture(*pass.p.srb, key, tex);
    }

    if(depthTex
       && (input.flags & Flag::SamplableDepth) == Flag::SamplableDepth
       && sampler_idx + 1 < (int)m_inputSamplers.size())
    {
      auto& depthSampl = m_inputSamplers[sampler_idx + 1];
      if(depthSampl.texture != depthTex)
      {
        depthSampl.texture = depthTex;
        auto* depthKey = srbKey(sampler_idx + 1);
        for(auto& [e, pass] : m_passes)
          if(pass.p.srb)
            score::gfx::replaceTexture(*pass.p.srb, depthKey, depthTex);
      }
    }
  }
}

QRhiTexture* RenderedRawRasterPipelineNode::textureForOutput(const Port& output)
{
  if(!m_hasMRT)
    return nullptr;

  // Find which output port index this is
  const auto& outputs = n.descriptor().outputs;
  for(int i = 0; i < (int)n.output.size() && i < (int)outputs.size(); i++)
  {
    if(n.output[i] == &output)
    {
      // Depth outputs expose the depth attachment directly. With
      // EXECUTION_MODEL: PER_LAYER on a depth target this is the
      // multi-layer Texture2DArray populated layer-by-layer via the
      // scratch+copy dance in runInitialPasses; for single-layer
      // depth shaders (shadow_map.frag) it's the plain 2D depth
      // texture. Either way, downstream wires it through
      // SceneResourceRoute(ShadowMapArray) into scene_state.
      if(outputs[i].type == "depth")
        return m_mrtRenderTarget.depthTexture;

      // Color output: index 0 = primary texture, 1+ = additional
      int colorIdx = 0;
      for(int j = 0; j < i; j++)
        if(outputs[j].type != "depth")
          colorIdx++;

      // CUBEMAP + MULTIVIEW shim: the public handle is the CubeMap,
      // not the shadow TextureArray that we actually render into.
      // Consumers bind this as samplerCube without knowing about the
      // array-then-copy dance happening under the hood.
      if(colorIdx == m_cubeCopyOutputIdx && m_cubeCopyCube)
        return m_cubeCopyCube;

      if(colorIdx == 0)
        return m_mrtRenderTarget.texture;
      else if(colorIdx - 1 < (int)m_mrtRenderTarget.additionalColorTextures.size())
        return m_mrtRenderTarget.additionalColorTextures[colorIdx - 1];
    }
  }
  return nullptr;
}

std::vector<Sampler> RenderedRawRasterPipelineNode::allSamplers() const noexcept
{
  // Input ports
  std::vector<Sampler> samplers = m_inputSamplers;

  // Apply non-owning per-port sampler overrides published by upstream
  // geometry's auxiliary_texture::sampler_handle (e.g., the per-bucket
  // QRhiSampler from ScenePreprocessor's per-glTF-texture sampler
  // config). The override is applied only on the SRB-build copy here;
  // m_inputSamplers itself keeps its original (owning) sampler so
  // release() can `delete sampler.sampler` without freeing a registry-
  // owned sampler.
  const std::size_t n_overrides
      = std::min(samplers.size(), m_inputSamplerOverrides.size());
  for(std::size_t i = 0; i < n_overrides; ++i)
  {
    if(m_inputSamplerOverrides[i])
      samplers[i].sampler = m_inputSamplerOverrides[i];
  }

  // Audio textures
  samplers.insert(samplers.end(), m_audioSamplers.begin(), m_audioSamplers.end());

  return samplers;
}

void RenderedRawRasterPipelineNode::initPass(
    const TextureRenderTarget& renderTarget, RenderList& renderer,
    QRhiResourceUpdateBatch& res, Edge& edge)
{
  auto& model_passes = n.descriptor().passes;
  SCORE_ASSERT(model_passes.size() == 1);

  QRhi& rhi = *renderer.state.rhi;

  QRhiBuffer* pubo{};
  pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("RenderedRawRasterPipelineNode::initPass::pubo");
  pubo->create();

  // Create the main pass
  try
  {
    auto [v, s] = score::gfx::makeShaders(renderer.state, n.m_vertexS, n.m_fragmentS);

    auto& mat
        = *reinterpret_cast<PipelineChangingMaterial*>(m_prevPipelineChangingMaterial);

    int max_binding = 3;
    auto samplers = allSamplers();
    if(!samplers.empty())
      max_binding += samplers.size();

    // Build additional bindings: auxiliary SSBOs + model UBO
    const auto bindingStages = QRhiShaderResourceBinding::StageFlag::VertexStage
                               | QRhiShaderResourceBinding::StageFlag::FragmentStage;

    ossia::small_vector<QRhiShaderResourceBinding, 4> additionalBindings;

    // INPUTS storage trio (storage_input SSBO / csf_image_input image2D /
    // uniform_input UBO) — order MUST match isf_emit_graphics_storage's
    // GLSL emission (declaration order, sequential bindings starting at
    // max_binding == 3 + samplers count).
    {
      auto extras = buildExtraBindings(m_storage);
      for(const auto& b : extras)
      {
        additionalBindings.push_back(b);
        max_binding++;
      }
    }

    for(auto& aux : m_auxiliarySSBOs)
    {
      // If no buffer yet, create a small dummy so the descriptor set is valid.
      // Dummy usage flag matches the aux kind so the created buffer can be
      // bound as the intended descriptor type.
      if(!aux.buffer)
      {
        auto usage = aux.is_uniform ? QRhiBuffer::UniformBuffer
                                    : QRhiBuffer::StorageBuffer;
        const int64_t dummySize = aux.is_uniform ? 256 : 16;
        auto* dummy = rhi.newBuffer(QRhiBuffer::Immutable, usage, dummySize);
        dummy->setName(aux.is_uniform ? "RRP_ubo_dummy" : "RRP_aux_dummy");
        dummy->create();
        aux.buffer = dummy;
        aux.size = dummySize;
        aux.owned = true;
      }

      // Persistent ping-pong pair: emit the read-only <name>_prev binding
      // FIRST (binding N), then the writable <name> binding (binding N+1).
      // GLSL emission uses the same ordering.
      if(aux.persistent && aux.prev_buffer)
      {
        additionalBindings.push_back(
            QRhiShaderResourceBinding::bufferLoad(
                max_binding, bindingStages, aux.prev_buffer));
        aux.prev_binding = max_binding;
        max_binding++;
      }

      QRhiShaderResourceBinding binding;
      if(aux.is_uniform)
      {
        // uniform_input → std140 UBO binding
        binding = QRhiShaderResourceBinding::uniformBuffer(
            max_binding, bindingStages, aux.buffer);
      }
      else if(aux.access == "read_only")
        binding = QRhiShaderResourceBinding::bufferLoad(
            max_binding, bindingStages, aux.buffer);
      else if(aux.access == "write_only")
        binding = QRhiShaderResourceBinding::bufferStore(
            max_binding, bindingStages, aux.buffer);
      else
        binding = QRhiShaderResourceBinding::bufferLoadStore(
            max_binding, bindingStages, aux.buffer);

      additionalBindings.push_back(binding);
      aux.binding = max_binding;  // remember slot for per-sub-mesh patching
      max_binding++;
    }

    // Auxiliary texture / storage-image bindings: placed right after
    // aux SSBOs, matching GLSL emission order. Dispatch on is_storage
    // so TYPE:"image" gets sampledTexture and TYPE:"storage_image"
    // gets imageLoad / imageStore / imageLoadStore per `access`.
    for(auto& ats : m_auxTextureSamplers)
    {
      QRhiShaderResourceBinding b;
      if(ats.is_storage)
      {
        if(ats.access == "read_only")
          b = QRhiShaderResourceBinding::imageLoad(
              max_binding, bindingStages, ats.texture, 0);
        else if(ats.access == "write_only")
          b = QRhiShaderResourceBinding::imageStore(
              max_binding, bindingStages, ats.texture, 0);
        else
          b = QRhiShaderResourceBinding::imageLoadStore(
              max_binding, bindingStages, ats.texture, 0);
      }
      else
      {
        b = QRhiShaderResourceBinding::sampledTexture(
            max_binding, bindingStages, ats.texture, ats.sampler);
      }
      additionalBindings.push_back(b);
      ats.binding = max_binding;
      max_binding++;
    }

    additionalBindings.push_back(QRhiShaderResourceBinding::uniformBuffer(
        max_binding, bindingStages, m_modelUBO));

    auto bindings = createDefaultBindings(
        renderer, renderTarget, pubo, m_materialUBO, allSamplers(),
        std::span<QRhiShaderResourceBinding>(
            additionalBindings.data(), additionalBindings.size()));

    auto& rhi = *renderer.state.rhi;
    auto ps = rhi.newGraphicsPipeline();
    ps->setName("RenderedRawRasterPipelineNode::initPass::ps");
    SCORE_ASSERT(ps);

    // Use the actual RT sample count whenever queryable — never assume
    // renderer.samples() — so the pipeline matches the render target it
    // will be bound to. They can differ if an RT was degraded for
    // samplable-depth + MSAA combos. -1 means the RT didn't carry enough
    // information (placeholder), in which case we trust renderer.samples().
    const int rtSamples = renderTarget.sampleCount();
    const int pipelineSamples = (rtSamples > 0) ? rtSamples : renderer.samples();
    if(rtSamples > 0 && rtSamples != renderer.samples())
    {
      qWarning() << "RawRaster::initPass: RT sampleCount=" << rtSamples
                 << "differs from renderer.samples()=" << renderer.samples();
    }
    ps->setSampleCount(pipelineSamples);

    // Procedural draws (VERTEX_INPUTS: [] + VERTEX_COUNT) don't need
    // a mesh — skip preparePipeline (no vertex-input layout bindings
    // to set).
    if(m_mesh)
      m_mesh->preparePipeline(*ps);

    // Compute effective pipeline state: the descriptor's PIPELINE_STATE (if
    // any) wins over the legacy material-UBO-driven blend. When no state is
    // declared (empty pipeline_state) we keep the legacy behaviour: blending
    // driven by the material's runtime-editable blend UI + hardcoded depth
    // test/write. This preserves bit-exact output for existing shaders.
    const auto& desc = n.m_descriptor;
    const bool hasDescriptorState = stateAffectsPipeline(desc.default_state);

    if(hasDescriptorState)
    {
      // New path: pipeline_state drives blend/depth/cull/stencil. Seed the
      // legacy material-UBO-driven blend on every attachment first so that
      // a partial PIPELINE_STATE declaration (e.g. just CULL_MODE) doesn't
      // silently lose the runtime blend UI's effect; applyPipelineState only
      // overrides blend when BLEND was explicitly declared.
      QRhiGraphicsPipeline::TargetBlend seededBlend;
      seededBlend.enable = mat.enable_blend;
      seededBlend.srcColor = mat.src_color;
      seededBlend.dstColor = mat.dst_color;
      seededBlend.opColor = mat.op_color;
      seededBlend.srcAlpha = mat.src_alpha;
      seededBlend.dstAlpha = mat.dst_alpha;
      seededBlend.opAlpha = mat.op_alpha;
      QList<QRhiGraphicsPipeline::TargetBlend> seedBlends;
      for(int i = 0; i < std::max(1, renderTarget.colorAttachmentCount()); i++)
        seedBlends.append(seededBlend);
      ps->setTargetBlends(seedBlends.begin(), seedBlends.end());
      ps->setDepthTest(true);
      ps->setDepthWrite(true);
      // Reverse-Z project rule (applyPipelineState overrides only if the
      // shader explicitly declares depth_compare).
      ps->setDepthOp(QRhiGraphicsPipeline::Greater);

      const bool depthAvailable
          = (renderTarget.depthTexture != nullptr)
            || (renderTarget.depthRenderBuffer != nullptr)
            || (renderTarget.msDepthTexture != nullptr);
      applyPipelineState(
          *ps, desc.default_state, renderTarget.colorAttachmentCount(),
          depthAvailable, /*wantsDepthByDefault=*/true);
    }
    else
    {
      // Legacy path: blend from material UBO, depth hardcoded on.
      QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;
      premulAlphaBlend.enable = mat.enable_blend;
      premulAlphaBlend.srcColor = mat.src_color;
      premulAlphaBlend.dstColor = mat.dst_color;
      premulAlphaBlend.opColor = mat.op_color;
      premulAlphaBlend.srcAlpha = mat.src_alpha;
      premulAlphaBlend.dstAlpha = mat.dst_alpha;
      premulAlphaBlend.opAlpha = mat.op_alpha;
      ps->setTargetBlends({premulAlphaBlend});

      ps->setDepthTest(true);
      ps->setDepthWrite(true);
      // Reverse-Z project rule.
      ps->setDepthOp(QRhiGraphicsPipeline::Greater);
    }

    // Topology is always runtime-controllable via the material UBO.
    switch(mat.mode)
    {
      default:
      case 0:
        ps->setTopology(QRhiGraphicsPipeline::Triangles);
        break;
      case 1:
        ps->setTopology(QRhiGraphicsPipeline::Points);
        break;
      case 2:
        ps->setTopology(QRhiGraphicsPipeline::Lines);
        break;
    }

    // Remap vertex inputs by semantic: match shader input variable names
    // to geometry attribute semantics. Honour explicit SEMANTIC overrides
    // declared on VERTEX_INPUTS in the descriptor (CSF-style). Skip for
    // procedural draws (no mesh, no attributes to remap).
    //
    // The fallback-aware overload resolves "REQUIRED: false" inputs
    // missing from upstream geometry to a shared PerInstance identity
    // buffer from the RenderList's pool. When no inputs opted in, the
    // plan is empty and the draw path short-circuits with zero cost.
    FallbackBindingPlan fallbackPlan;
    if(m_mesh)
    {
      if(auto* geom = m_mesh->semanticGeometry())
      {
        if(!remapPipelineVertexInputs(
               *ps, v, *geom, n.descriptor(),
               rhi, renderer.vertexFallbackPool(), res, fallbackPlan))
        {
          delete ps;
          delete pubo;
          return;
        }
      }
    }

    ps->setShaderStages({{QRhiShaderStage::Vertex, v}, {QRhiShaderStage::Fragment, s}});

    ps->setShaderResourceBindings(bindings);

    SCORE_ASSERT(renderTarget.renderPass);
    ps->setRenderPassDescriptor(renderTarget.renderPass);

    if(!ps->create())
    {
      qDebug() << "Warning! Pipeline not created";
      delete ps;
      ps = nullptr;
    }

    Pipeline pip = {ps, bindings};
    if(pip.pipeline)
    {
      Pass pass{renderTarget, pip, pubo};
      pass.fallback_bindings = std::move(fallbackPlan);
      m_passes.emplace_back(&edge, std::move(pass));
    }
    else
    {
      delete pubo;
    }
  }
  catch(...)
  {
    delete pubo;
  }
}

void RenderedRawRasterPipelineNode::initMRTPass(
    RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;
  const auto& outputs = n.descriptor().outputs;

  // Tear down any state left from a previous init pass. `update` calls
  // `m_mrtRenderTarget.release()` before hitting us again, but it's not
  // responsible for our private per-mip / per-face RT pool or the
  // CUBEMAP+MULTIVIEW shim's separate cube handle. Without these drops
  // the pool would grow unboundedly across re-inits and, worse,
  // m_mipRTs entries would point at a shadow array that's already been
  // freed — the next beginPass on one of those stale RTs triggers a
  // driver-level crash in CmdBeginRenderPass (NVIDIA specifically).
  for(auto& e : m_mipRTs)
  {
    if(e.renderTarget)
      e.renderTarget->deleteLater();
    if(e.renderPass)
      e.renderPass->deleteLater();
    if(e.depth)
      e.depth->deleteLater();
  }
  m_mipRTs.clear();
  m_mipCount = 0;

  // PerLayer depth-path resources. The color path's per-layer RTs are
  // owned by m_mipRTs (cleared above); the shared scratch depth + RT
  // used by the depth path live outside m_mipRTs and must be dropped
  // explicitly here. m_perLayerOutputDepthArray aliases depthTex (owned
  // by m_mrtRenderTarget) so it just gets nulled out.
  if(m_perLayerSharedRT)
  {
    m_perLayerSharedRT->deleteLater();
    m_perLayerSharedRT = nullptr;
  }
  if(m_perLayerSharedRP)
  {
    m_perLayerSharedRP->deleteLater();
    m_perLayerSharedRP = nullptr;
  }
  if(m_perLayerScratchDepth)
  {
    m_perLayerScratchDepth->deleteLater();
    m_perLayerScratchDepth = nullptr;
  }
  if(m_perLayerDummyColor)
  {
    m_perLayerDummyColor->deleteLater();
    m_perLayerDummyColor = nullptr;
  }
  m_perLayerOutputDepthArray = nullptr;
  m_perLayerOutputIndex = -1;
  m_perLayerIsDepth = false;

  if(m_cubeCopyCube)
  {
    m_cubeCopyCube->deleteLater();
    m_cubeCopyCube = nullptr;
  }
  // m_cubeCopyShadowArray is a pointer into m_mrtRenderTarget's
  // attachments; it's freed by m_mrtRenderTarget.release() in update().
  m_cubeCopyShadowArray = nullptr;
  m_cubeCopyOutputIdx = -1;

  // Per-invocation UBO+SRB pool — rebuilt below against the fresh
  // main SRB once the pipeline is re-created. Leaking these across
  // re-inits would point old SRBs at freed buffers (same failure
  // mode as the stale mip RTs above).
  for(auto* ubo : m_perInvocationUBOs)
    if(ubo) ubo->deleteLater();
  m_perInvocationUBOs.clear();
  for(auto* srb : m_perInvocationSRBs)
    if(srb) srb->deleteLater();
  m_perInvocationSRBs.clear();

  // Target size resolution: honour OUTPUTS.WIDTH / HEIGHT (integer
  // literal or string expression) when declared; otherwise fall back
  // to the renderer's render-size. A RAW_RASTER_PIPELINE shader has
  // one shared render pass, so all attachments end up at the same
  // size — pick the first OUTPUT with an explicit size as the RT
  // size. Mixing sized and unsized outputs is fine (unsized ones
  // just inherit); mixing differing explicit sizes is a shader-
  // author error we don't diagnose here.
  QSize sz = renderer.state.renderSize;
  // First non-zero explicit WIDTH/HEIGHT wins. Depth outputs participate
  // too: shadow_cascades.frag (depth-only, no colour outputs at all)
  // declares the shadow-map resolution on its depth output, and we want
  // that to drive the RT size rather than falling through to renderSize.
  for(const auto& out : outputs)
  {
    int w = out.width_expression.empty()
                ? out.width
                : resolveIntExpression(out.width_expression, 0);
    int h = out.height_expression.empty()
                ? out.height
                : resolveIntExpression(out.height_expression, 0);
    if(w > 0 && h > 0)
    {
      sz = QSize(w, h);
      break;
    }
  }

  // EXECUTION_MODEL resolution. Matters before allocation because
  // PER_MIP forces a MipMapped flag on the target output's texture,
  // PER_CUBE_FACE forces a CubeMap flag. Manual / Single have no
  // effect on allocation — they only influence the render loop in
  // runInitialPasses().
  {
    const auto& em = n.descriptor().execution_model;
    std::string et = em.type;
    for(auto& c : et)
      c = (char)std::toupper((unsigned char)c);
    if(et == "PER_MIP")
      m_executionMode = ExecutionMode::PerMip;
    else if(et == "PER_CUBE_FACE")
      m_executionMode = ExecutionMode::PerCubeFace;
    else if(et == "PER_LAYER")
      m_executionMode = ExecutionMode::PerLayer;
    else if(et == "MANUAL")
      m_executionMode = ExecutionMode::Manual;
    else
      m_executionMode = ExecutionMode::Single;

    m_perMipOutputIndex = -1;
    m_perCubeFaceOutputIndex = -1;
    m_perLayerOutputIndex = -1;
    m_perLayerIsDepth = false;
    const bool needsTarget = m_executionMode == ExecutionMode::PerMip
                             || m_executionMode == ExecutionMode::PerCubeFace
                             || m_executionMode == ExecutionMode::PerLayer;
    if(needsTarget && !em.target.empty())
    {
      // PER_MIP / PER_CUBE_FACE only make sense on colour outputs (depth
      // attachments don't have mip chains in our pipeline, and cube
      // depth would need a separate code path). PER_LAYER allows either:
      // colour TextureArray (setLayer attachment) or depth TextureArray
      // (scratch + copy strategy). Walk the raw outputs[] for PER_LAYER
      // so depth entries are included; keep the colour-only walk for the
      // other two modes.
      if(m_executionMode == ExecutionMode::PerLayer)
      {
        for(int i = 0; i < (int)outputs.size(); ++i)
        {
          if(outputs[i].name == em.target)
          {
            m_perLayerOutputIndex = i;
            m_perLayerIsDepth = (outputs[i].type == "depth");
            break;
          }
        }
      }
      else
      {
        int colorIdx = 0;
        for(const auto& out : outputs)
        {
          if(out.type == "depth")
            continue;
          if(out.name == em.target)
          {
            if(m_executionMode == ExecutionMode::PerMip)
              m_perMipOutputIndex = colorIdx;
            else
              m_perCubeFaceOutputIndex = colorIdx;
            break;
          }
          ++colorIdx;
        }
      }
      const bool resolved
          = (m_executionMode == ExecutionMode::PerMip
             && m_perMipOutputIndex >= 0)
            || (m_executionMode == ExecutionMode::PerCubeFace
                && m_perCubeFaceOutputIndex >= 0)
            || (m_executionMode == ExecutionMode::PerLayer
                && m_perLayerOutputIndex >= 0);
      if(!resolved)
      {
        qWarning() << "RawRaster EXECUTION_MODEL=" << et.c_str()
                   << ": TARGET" << QString::fromStdString(em.target)
                   << "not found among outputs — falling back to SINGLE";
        m_executionMode = ExecutionMode::Single;
      }
    }

    // PER_CUBE_FACE + MULTIVIEW on the same shader is redundant:
    // multiview already amplifies one draw into 6 face writes, so
    // iterating per face would collapse back to the same 6 writes.
    // Warn and disable the per-face loop — the cube-copy shim
    // (CUBEMAP + MULTIVIEW) handles everything downstream.
    if(m_executionMode == ExecutionMode::PerCubeFace
       && n.descriptor().multiview_count >= 2)
    {
      qWarning()
          << "RawRaster EXECUTION_MODEL=PER_CUBE_FACE + MULTIVIEW:"
          << n.descriptor().multiview_count
          << "is redundant. Multiview already amplifies one draw to"
             " N faces; PER_CUBE_FACE is for the explicit 6-pass path"
             " without multiview. Disabling PER_CUBE_FACE.";
      m_executionMode = ExecutionMode::Single;
      m_perCubeFaceOutputIndex = -1;
    }
  }

  // Layered / multiview detection — same shape as SimpleRenderedISFNode.
  // `LAYERS: N` on any OUTPUT → N-layer texture array; `MULTIVIEW: N` on
  // the descriptor → single-draw-writes-N-views (requires caps.multiview).
  // Consumer shaders like `prefilter_ggx.frag` / `irradiance_convolve.frag`
  // / `shadow_cascades.frag` all rely on this plumbing to land their
  // outputs on the right cubemap face / cascade slice.
  int maxLayers = 1;
  for(const auto& out : outputs)
    if(out.layers > maxLayers)
      maxLayers = out.layers;
  const int mvCount = n.descriptor().multiview_count;
  const bool wantMultiview
      = mvCount >= 2 && renderer.state.caps.multiview;
  if(wantMultiview && mvCount > maxLayers)
    maxLayers = mvCount;

  // MSAA uniform across colour attachments — pick the max SAMPLES declared
  // by any OUTPUT and apply it to the render pass. Allocated textures stay
  // single-sample and serve as MSAA resolve targets (see SimpleRenderedISF
  // initMRTPass for the full rationale).
  int mrtSamples = std::max(renderer.samples(), 1);
  for(const auto& out : outputs)
    mrtSamples = std::max(mrtSamples, out.samples);

  // Allocate colour + depth textures per declared OUTPUT. Unknown / empty
  // FORMAT falls back to RGBA8 (colour) or D32F (depth). `type: "depth"`
  // skips the standard depth-renderbuffer path and uses this texture as
  // the depth attachment — required for shadow-map passes that want to
  // sample the depth array downstream.
  std::vector<QRhiTexture*> colorTextures;
  QRhiTexture* depthTex = nullptr;

  // Resolve the colour-attachment index of the PER_MIP / PER_CUBE_FACE
  // target up-front (walk order matches the colorTextures[] we're
  // about to build) so the allocation pass can OR in the matching
  // flag only for that texture.
  const int perMipColorIdx
      = (m_executionMode == ExecutionMode::PerMip) ? m_perMipOutputIndex
                                                   : -1;
  const int perCubeFaceColorIdx
      = (m_executionMode == ExecutionMode::PerCubeFace)
            ? m_perCubeFaceOutputIndex
            : -1;
  int colorAllocIdx = 0;
  // Reset the cube-copy shim state; (re)assigned below when an output
  // with CUBEMAP:true + MULTIVIEW:N is encountered.
  m_cubeCopyOutputIdx = -1;
  m_cubeCopyShadowArray = nullptr;
  m_cubeCopyCube = nullptr;

  for(const auto& out : outputs)
  {
    if(out.type == "depth")
    {
      auto depthFmt = score::gfx::parseOutputFormat(out.format, QRhiTexture::D32F);
      QRhiTexture::Flags dflags = QRhiTexture::RenderTarget;
      if(maxLayers > 1)
      {
        dflags |= QRhiTexture::TextureArray;
        depthTex = rhi.newTextureArray(depthFmt, maxLayers, sz, 1, dflags);
      }
      else
      {
        depthTex = rhi.newTexture(depthFmt, sz, 1, dflags);
      }
      depthTex->setName(
          ("RenderedRawRasterPipelineNode::MRT::depth::" + out.name).c_str());
      SCORE_ASSERT(depthTex->create());
    }
    else
    {
      auto fmt = score::gfx::parseOutputFormat(out.format, QRhiTexture::RGBA8);
      QRhiTexture::Flags flags
          = QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore;
      const int layers
          = std::max({1, out.layers, (wantMultiview ? mvCount : 1),
                       (out.is_cubemap ? 6 : 1)});
      // PER_MIP: flag the target output so QRhi allocates the full mip
      // chain. Downstream consumers that care about the mips (prefilter
      // sampling keyed on roughness) need them, and the per-mip render
      // targets built below attach individual levels.
      if(colorAllocIdx == perMipColorIdx)
        flags |= QRhiTexture::MipMapped;

      // GENERATE_MIPS: MipMapped allocation + UsedWithGenerateMips flag
      // so QRhi's generateMips() can filter the base level into the
      // sub-mips at end-of-frame. Orthogonal to PER_MIP (which provides
      // shader-authored per-mip content) — we just need the storage
      // shape + the capability bit.
      if(out.generate_mips)
        flags |= QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips;
      QRhiTexture* tex = nullptr;

      // Transparent CUBEMAP + MULTIVIEW path. QRhi forbids multiview on
      // a cube texture (qrhi.cpp:2561-2565), so we render into a
      // `UsedAsTransferSource`-tagged 2D TextureArray (what multiview
      // accepts) and stamp a separate CubeMap alongside for downstream
      // sampling. After the render pass ends we copyTexture each array
      // layer into the matching cube face — downstream sees a real
      // samplerCube without the shader having to know about it.
      // Only one output gets the cube-copy treatment in this first cut
      // (multiview already amortises 6× render amplification for free).
      const bool wantCubeCopy
          = out.is_cubemap && wantMultiview && m_cubeCopyOutputIdx < 0;

      // PER_CUBE_FACE target: allocate as a real CubeMap (6 implicit
      // layers). setLayer(face) per per-face render target drives each
      // loop iteration. Mutually exclusive with the multiview-cube-copy
      // shim above: PER_CUBE_FACE assumes you want the 6-pass behaviour
      // explicitly; multiview would collapse the 6 passes back into 1.
      const bool useCubeDirect
          = (colorAllocIdx == perCubeFaceColorIdx)
            || (out.is_cubemap && !wantMultiview);

      if(wantCubeCopy)
      {
        // Cubemaps must have square faces in QRhi / Vulkan (CUBE_COMPATIBLE
        // images require extent.width == extent.height). When the render
        // target size is non-square (typical window aspect), the cube we
        // hand downstream would otherwise be non-cubemap-compatible and
        // produce stripe-like artefacts from the copy/sample stride
        // mismatch. Force the cube face to min(w, h); the shadow array is
        // sized to match so the multiview draw writes the full face.
        const int face_edge = std::min(sz.width(), sz.height());
        const QSize cubeSz(face_edge, face_edge);

        // The rendered-to shadow array. Multiview-compatible shape, square
        // (matches the cube). UsedAsTransferSource so it can be a
        // copyTexture source.
        QRhiTexture::Flags arrayFlags = flags | QRhiTexture::TextureArray
                                        | QRhiTexture::UsedAsTransferSource;
        tex = rhi.newTextureArray(fmt, 6, cubeSz, 1, arrayFlags);
        tex->setName(
            ("RRPNode::MRT::cubeCopyArray::" + out.name).c_str());
        SCORE_ASSERT(tex->create());
        m_cubeCopyShadowArray = tex;

        // The downstream-visible cube. Same format, no RenderTarget
        // flag (we never render into it directly, only copy). Default
        // access is sampled/transfer-dst — enough for the classic
        // consumer path (samplerCube). MipMapped is forwarded so a
        // future prefilter chain can be generated downstream if the
        // user also requested it on this output. UsedWithGenerateMips
        // lets the end-of-frame generateMips() hit the public cube
        // (the shadow array isn't sampled downstream so it doesn't
        // need the flag itself).
        QRhiTexture::Flags cubeFlags = QRhiTexture::CubeMap;
        if(flags & QRhiTexture::MipMapped)
          cubeFlags |= QRhiTexture::MipMapped;
        if(out.generate_mips)
          cubeFlags |= QRhiTexture::UsedWithGenerateMips;
        QRhiTexture* cube = rhi.newTexture(fmt, cubeSz, 1, cubeFlags);
        cube->setName(
            ("RRPNode::MRT::cubeCopyCube::" + out.name).c_str());
        SCORE_ASSERT(cube->create());
        m_cubeCopyCube = cube;
        m_cubeCopyOutputIdx = colorAllocIdx;
      }
      else if(useCubeDirect)
      {
        flags |= QRhiTexture::CubeMap;
        // QRhi: a cubemap is allocated via newTexture (not newTextureArray)
        // — its 6 faces are implicit when the CubeMap flag is set. A cube
        // array (multiple cubes) would need newTextureArray + CubeMap, but
        // we only cover single-cube here.
        tex = rhi.newTexture(fmt, sz, 1, flags);
      }
      else if(layers > 1)
      {
        flags |= QRhiTexture::TextureArray;
        tex = rhi.newTextureArray(fmt, layers, sz, 1, flags);
      }
      else
      {
        tex = rhi.newTexture(fmt, sz, 1, flags);
      }

      if(!wantCubeCopy)
      {
        tex->setName(
            ("RRPNode::MRT::color::" + out.name).c_str());
        SCORE_ASSERT(tex->create());
      }
      colorTextures.push_back(tex);
      ++colorAllocIdx;
    }
  }

  // Render-target variant picked from the shape of the declared outputs.
  // Raw Raster always ships with depth test/write (3D geometry invariant),
  // so on the common colour-only path we still synthesise a depth target
  // if the shader didn't declare one explicitly.
  if(colorTextures.empty() && depthTex)
  {
    // Depth-only shader (e.g. shadow_cascades.frag).
    m_mrtRenderTarget = createDepthOnlyRenderTarget(
        renderer.state, sz, mrtSamples, /*samplableDepth=*/true,
        depthTex->format());
    if(m_mrtRenderTarget.depthTexture
       && m_mrtRenderTarget.depthTexture != depthTex)
    {
      m_mrtRenderTarget.depthTexture->deleteLater();
    }
    m_mrtRenderTarget.depthTexture = depthTex;
  }
  else if(wantMultiview && !colorTextures.empty())
  {
    // Allocate depth for the multiview RT if the shader didn't declare
    // one — createMultiViewRenderTarget expects a matching layered depth
    // or nullptr. Layered depth is cheaper and Vulkan-correct for MV.
    if(!depthTex)
    {
      depthTex = rhi.newTextureArray(
          QRhiTexture::D32F, mvCount, sz, 1,
          QRhiTexture::RenderTarget | QRhiTexture::TextureArray);
      depthTex->setName(
          "RenderedRawRasterPipelineNode::MRT::depthTextureArray (D32F)");
      SCORE_ASSERT(depthTex->create());
    }
    m_mrtRenderTarget = createMultiViewRenderTarget(
        renderer.state, colorTextures[0], mvCount, depthTex, mrtSamples);
    for(std::size_t i = 1; i < colorTextures.size(); ++i)
      m_mrtRenderTarget.additionalColorTextures.push_back(colorTextures[i]);
  }
  else if(maxLayers > 1 && !colorTextures.empty())
  {
    // Layered but not multiview — render to layer 0 by default; downstream
    // per-pass LAYER selection (once PASSES loop lands) will pick others.
    m_mrtRenderTarget = createLayeredRenderTarget(
        renderer.state, colorTextures[0], 0, depthTex, mrtSamples);
    for(std::size_t i = 1; i < colorTextures.size(); ++i)
      m_mrtRenderTarget.additionalColorTextures.push_back(colorTextures[i]);
  }
  else if(!colorTextures.empty())
  {
    // Plain MRT path — single-sample 2D textures, renderbuffer depth if
    // the shader didn't ask for a samplable depth OUTPUT.
    if(depthTex)
    {
      m_mrtRenderTarget = createRenderTarget(
          renderer.state,
          std::span<QRhiTexture* const>{
              colorTextures.data(), colorTextures.size()},
          depthTex, mrtSamples);
    }
    else
    {
      m_mrtRenderTarget.texture = colorTextures[0];
      for(std::size_t i = 1; i < colorTextures.size(); i++)
        m_mrtRenderTarget.additionalColorTextures.push_back(colorTextures[i]);

      QList<QRhiColorAttachment> attachments;
      for(auto* tex : colorTextures)
        attachments.append(QRhiColorAttachment(tex));

      QRhiTextureRenderTargetDescription desc;
      desc.setColorAttachments(attachments.begin(), attachments.end());

      // Reverse-Z project rule: D32F float depth. D24 + reverse-Z is strictly
      // worse than standard-Z. Stencil dropped (unused elsewhere).
      m_mrtRenderTarget.depthTexture = rhi.newTexture(
          QRhiTexture::D32F, sz, renderer.samples(),
          QRhiTexture::RenderTarget);
      m_mrtRenderTarget.depthTexture->setName(
          "RenderedRawRasterPipelineNode::MRT::depthTexture (D32F)");
      SCORE_ASSERT(m_mrtRenderTarget.depthTexture->create());
      desc.setDepthTexture(m_mrtRenderTarget.depthTexture);

      auto* renderTarget = rhi.newTextureRenderTarget(desc);
      renderTarget->setName("RenderedRawRasterPipelineNode::MRT::renderTarget");
      SCORE_ASSERT(renderTarget);

      auto* renderPass = renderTarget->newCompatibleRenderPassDescriptor();
      renderPass->setName("RenderedRawRasterPipelineNode::MRT::renderPass");
      SCORE_ASSERT(renderPass);

      renderTarget->setRenderPassDescriptor(renderPass);
      SCORE_ASSERT(renderTarget->create());

      m_mrtRenderTarget.renderTarget = renderTarget;
      m_mrtRenderTarget.renderPass = renderPass;
    }
  }
  else
  {
    return;
  }

  // PER_CUBE_FACE: build one render target per cube face, each
  // attaching the same cube texture via setLayer(i). Mirrors the
  // PER_MIP path structurally (iteration over a fixed axis with a
  // distinct per-iteration RT) but with a CubeMap target instead of
  // a MipMapped one. m_mipRTs reused as storage (semantics: index =
  // face in this mode, mip level in PER_MIP mode). MUTUALLY EXCLUSIVE
  // with PER_MIP — PER_CUBE_FACE_MIP would require a 2D iteration
  // and isn't supported here; compose via external looping if needed.
  if(m_executionMode == ExecutionMode::PerCubeFace
     && m_perCubeFaceOutputIndex >= 0 && !colorTextures.empty())
  {
    QRhiTexture* targetTex
        = (m_perCubeFaceOutputIndex == 0)
              ? m_mrtRenderTarget.texture
              : (m_perCubeFaceOutputIndex - 1
                         < (int)m_mrtRenderTarget.additionalColorTextures.size()
                     ? m_mrtRenderTarget.additionalColorTextures
                           [m_perCubeFaceOutputIndex - 1]
                     : nullptr);

    if(targetTex)
    {
      m_mipCount = 6;  // m_mipCount stores invocation count for the loop
      m_mipRTs.reserve(6);
      const QSize faceSize = targetTex->pixelSize();

      for(int face = 0; face < 6; ++face)
      {
        QRhiColorAttachment color(targetTex);
        color.setLayer(face);
        // No multiview here: PER_CUBE_FACE opts into per-pass cube
        // rendering explicitly. Multiview + cubemap is forbidden by
        // QRhi anyway.

        QRhiTexture* faceDepth = rhi.newTexture(
            QRhiTexture::D32F, faceSize, 1, QRhiTexture::RenderTarget);
        faceDepth->setName(
            ("RRPNode::MRT::perCubeFaceDepth::"
             + std::to_string(face))
                .c_str());
        SCORE_ASSERT(faceDepth->create());

        QRhiTextureRenderTargetDescription faceDesc;
        faceDesc.setColorAttachments({color});
        faceDesc.setDepthTexture(faceDepth);

        auto* faceRT = rhi.newTextureRenderTarget(faceDesc);
        faceRT->setName(
            ("RRPNode::MRT::perCubeFaceRT::"
             + std::to_string(face))
                .c_str());
        auto* faceRP = faceRT->newCompatibleRenderPassDescriptor();
        faceRP->setName(
            ("RRPNode::MRT::perCubeFaceRP::"
             + std::to_string(face))
                .c_str());
        faceRT->setRenderPassDescriptor(faceRP);
        SCORE_ASSERT(faceRT->create());

        MipRT entry;
        entry.renderTarget = faceRT;
        entry.renderPass = faceRP;
        entry.depth = faceDepth;
        m_mipRTs.push_back(entry);
      }
    }
    else
    {
      qWarning() << "RawRaster EXECUTION_MODEL=PER_CUBE_FACE: could not "
                    "resolve target texture — falling back to SINGLE";
      m_executionMode = ExecutionMode::Single;
    }
  }

  // PER_MIP: build one render target per mip level of the target output,
  // each attaching that specific level via setLevel(i). The draw loop in
  // runInitialPasses() iterates these in order, injecting the mip index
  // via ProcessUBO.passIndex. Multiview propagates: when the shader
  // declared MULTIVIEW:6 (irradiance / prefilter cube case), each mip's
  // attachment also carries setMultiViewCount(6) so one draw writes all
  // six faces of that mip. Depth is a per-mip single-sample D32F to
  // keep the pipeline's render-pass contract consistent across levels.
  if(m_executionMode == ExecutionMode::PerMip && m_perMipOutputIndex >= 0
     && !colorTextures.empty())
  {
    QRhiTexture* targetTex
        = (m_perMipOutputIndex == 0)
              ? m_mrtRenderTarget.texture
              : (m_perMipOutputIndex - 1
                         < (int)m_mrtRenderTarget.additionalColorTextures.size()
                     ? m_mrtRenderTarget.additionalColorTextures
                           [m_perMipOutputIndex - 1]
                     : nullptr);

    if(targetTex)
    {
      QSize baseSize = targetTex->pixelSize();
      int mipCount = 1;
      {
        int s = std::min(baseSize.width(), baseSize.height());
        while(s > 1)
        {
          s >>= 1;
          ++mipCount;
        }
      }
      m_mipCount = mipCount;
      m_mipRTs.reserve(mipCount);

      for(int i = 0; i < mipCount; ++i)
      {
        QSize mipSize(
            std::max(1, baseSize.width() >> i),
            std::max(1, baseSize.height() >> i));

        QRhiColorAttachment color(targetTex);
        color.setLevel(i);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        if(wantMultiview)
          color.setMultiViewCount(mvCount);
#endif

        // Depth must match multiview shape: a plain 2D texture as the
        // depth attachment against a multiview color attachment fails
        // QRhi's render-pass compat check. Allocate a layered depth for
        // the multiview case, plain 2D otherwise. Each mip gets its own
        // depth because the attachment size must match the colour
        // attachment's mip-i pixel size.
        QRhiTexture* mipDepth = nullptr;
        if(wantMultiview)
        {
          mipDepth = rhi.newTextureArray(
              QRhiTexture::D32F, mvCount, mipSize, 1,
              QRhiTexture::RenderTarget | QRhiTexture::TextureArray);
        }
        else
        {
          mipDepth = rhi.newTexture(
              QRhiTexture::D32F, mipSize, 1, QRhiTexture::RenderTarget);
        }
        mipDepth->setName(
            ("RenderedRawRasterPipelineNode::MRT::perMipDepth::"
             + std::to_string(i))
                .c_str());
        SCORE_ASSERT(mipDepth->create());

        QRhiTextureRenderTargetDescription mipDesc;
        mipDesc.setColorAttachments({color});
        mipDesc.setDepthTexture(mipDepth);

        auto* mipRT = rhi.newTextureRenderTarget(mipDesc);
        mipRT->setName(
            ("RenderedRawRasterPipelineNode::MRT::perMipRT::"
             + std::to_string(i))
                .c_str());
        auto* mipRP = mipRT->newCompatibleRenderPassDescriptor();
        mipRP->setName(
            ("RenderedRawRasterPipelineNode::MRT::perMipRP::"
             + std::to_string(i))
                .c_str());
        mipRT->setRenderPassDescriptor(mipRP);
        SCORE_ASSERT(mipRT->create());

        MipRT entry;
        entry.renderTarget = mipRT;
        entry.renderPass = mipRP;
        entry.depth = mipDepth;
        m_mipRTs.push_back(entry);
      }
    }
    else
    {
      qWarning() << "RawRaster EXECUTION_MODEL=PER_MIP: could not resolve "
                    "target texture — falling back to SINGLE";
      m_executionMode = ExecutionMode::Single;
    }
  }

  // PER_LAYER: build one render target per layer of the target output's
  // TextureArray (or copy strategy for depth targets — see below). The
  // draw loop in runInitialPasses() iterates them in order, injecting
  // the layer index via ProcessUBO.passIndex. Drives shadow_cascades.
  //
  // Two paths depending on target type:
  //
  //   - COLOR target: same shape as PER_CUBE_FACE with a variable layer
  //     count. m_mipRTs holds N entries, each with QRhiColorAttachment
  //     bound via setLayer(i). Per-layer 2D depth (one D32F per slice)
  //     keeps the render-pass attachment shapes consistent.
  //
  //   - DEPTH target: Qt RHI 6.11 has no per-layer depth-attachment API
  //     (QRhiTextureRenderTargetDescription::setDepthTexture takes a
  //     QRhiTexture* with no layer overload). We render to a single
  //     shared scratch 2D D32F and copy it into layer i of the OUTPUT
  //     depth array after each iteration's endPass. The scratch is
  //     UsedAsTransferSource so the per-iteration copyTexture works.
  if(m_executionMode == ExecutionMode::PerLayer && m_perLayerOutputIndex >= 0)
  {
    const auto& targetOut = outputs[m_perLayerOutputIndex];
    const int layerCount = std::max(1, targetOut.layers);

    if(m_perLayerIsDepth)
    {
      // depthTex is the OUTPUT array (allocated as Texture2DArray
      // earlier when maxLayers > 1). m_perLayerOutputDepthArray
      // aliases it for the post-pass copy destination.
      if(depthTex && layerCount > 1)
      {
        m_perLayerOutputDepthArray = depthTex;

        const auto depthFmt = depthTex->format();
        m_perLayerScratchDepth = rhi.newTexture(
            depthFmt, sz, 1,
            QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
        m_perLayerScratchDepth->setName(
            ("RRPNode::MRT::perLayerScratch::" + targetOut.name).c_str());
        SCORE_ASSERT(m_perLayerScratchDepth->create());

        // Mirror createDepthOnlyRenderTarget's attachment shape so the
        // pipeline (created against m_mrtRenderTarget.renderPass, which
        // came from createDepthOnlyRenderTarget) is render-pass-
        // compatible with our shared RT. That helper attaches a 1×1
        // dummy RGBA8 color alongside the depth — required by GLES
        // backends and harmless on desktop. We allocate our own dummy
        // (rather than borrowing m_mrtRenderTarget.dummyColorTexture,
        // whose lifetime is owned by m_mrtRenderTarget) so the shared
        // RT here owns a self-contained set of attachments.
        m_perLayerDummyColor = rhi.newTexture(
            QRhiTexture::RGBA8, QSize(1, 1), 1, QRhiTexture::RenderTarget);
        m_perLayerDummyColor->setName(
            ("RRPNode::MRT::perLayerDummyColor::" + targetOut.name).c_str());
        SCORE_ASSERT(m_perLayerDummyColor->create());

        QRhiTextureRenderTargetDescription scratchDesc;
        {
          QRhiColorAttachment color0(m_perLayerDummyColor);
          scratchDesc.setColorAttachments({color0});
        }
        scratchDesc.setDepthTexture(m_perLayerScratchDepth);

        m_perLayerSharedRT = rhi.newTextureRenderTarget(scratchDesc);
        m_perLayerSharedRT->setName(
            ("RRPNode::MRT::perLayerSharedRT::" + targetOut.name).c_str());
        m_perLayerSharedRP
            = m_perLayerSharedRT->newCompatibleRenderPassDescriptor();
        m_perLayerSharedRP->setName(
            ("RRPNode::MRT::perLayerSharedRP::" + targetOut.name).c_str());
        m_perLayerSharedRT->setRenderPassDescriptor(m_perLayerSharedRP);
        SCORE_ASSERT(m_perLayerSharedRT->create());

        m_mipCount = layerCount;  // reuse for invocation count
      }
      else
      {
        qDebug()
            << "RawRaster EXECUTION_MODEL=PER_LAYER: depth target"
            << QString::fromStdString(targetOut.name)
            << "needs LAYERS > 1 — falling back to SINGLE";
        m_executionMode = ExecutionMode::Single;
      }
    }
    else
    {
      // Color path. Resolve the colour-attachment index from the raw
      // outputs[] index (depth entries don't take a colour slot).
      int colorIdx = 0;
      for(int j = 0; j < m_perLayerOutputIndex; ++j)
        if(outputs[j].type != "depth")
          ++colorIdx;

      QRhiTexture* targetTex
          = (colorIdx == 0)
                ? m_mrtRenderTarget.texture
                : (colorIdx - 1
                           < (int)m_mrtRenderTarget.additionalColorTextures.size()
                       ? m_mrtRenderTarget.additionalColorTextures[colorIdx - 1]
                       : nullptr);

      if(targetTex && layerCount > 1)
      {
        const QSize layerSize = targetTex->pixelSize();
        m_mipCount = layerCount;
        m_mipRTs.reserve(layerCount);

        for(int layer = 0; layer < layerCount; ++layer)
        {
          QRhiColorAttachment color(targetTex);
          color.setLayer(layer);

          // Per-layer 2D depth — same rationale as PER_CUBE_FACE: depth
          // attachment size must match the colour attachment, and a
          // layered depth here would force multi-view shape against a
          // single-layer colour binding.
          QRhiTexture* layerDepth = rhi.newTexture(
              QRhiTexture::D32F, layerSize, 1, QRhiTexture::RenderTarget);
          layerDepth->setName(
              ("RRPNode::MRT::perLayerDepth::" + std::to_string(layer))
                  .c_str());
          SCORE_ASSERT(layerDepth->create());

          QRhiTextureRenderTargetDescription layerDesc;
          layerDesc.setColorAttachments({color});
          layerDesc.setDepthTexture(layerDepth);

          auto* layerRT = rhi.newTextureRenderTarget(layerDesc);
          layerRT->setName(
              ("RRPNode::MRT::perLayerRT::" + std::to_string(layer))
                  .c_str());
          auto* layerRP = layerRT->newCompatibleRenderPassDescriptor();
          layerRP->setName(
              ("RRPNode::MRT::perLayerRP::" + std::to_string(layer))
                  .c_str());
          layerRT->setRenderPassDescriptor(layerRP);
          SCORE_ASSERT(layerRT->create());

          MipRT entry;
          entry.renderTarget = layerRT;
          entry.renderPass = layerRP;
          entry.depth = layerDepth;
          m_mipRTs.push_back(entry);
        }
      }
      else
      {
        qDebug()
            << "RawRaster EXECUTION_MODEL=PER_LAYER: colour target"
            << QString::fromStdString(targetOut.name)
            << "needs LAYERS > 1 and a resolved texture — falling back"
               " to SINGLE";
        m_executionMode = ExecutionMode::Single;
      }
    }
  }

  // Create the pipeline
  QRhiBuffer* pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("RenderedRawRasterPipelineNode::initMRTPass::pubo");
  pubo->create();

  try
  {
    auto [v, s] = score::gfx::makeShaders(renderer.state, n.m_vertexS, n.m_fragmentS);

    auto& mat
        = *reinterpret_cast<PipelineChangingMaterial*>(m_prevPipelineChangingMaterial);

    int max_binding = 3;
    auto samplers = allSamplers();
    if(!samplers.empty())
      max_binding += samplers.size();

    // Build additional bindings: auxiliary SSBOs + model UBO
    const auto bindingStages = QRhiShaderResourceBinding::StageFlag::VertexStage
                               | QRhiShaderResourceBinding::StageFlag::FragmentStage;

    ossia::small_vector<QRhiShaderResourceBinding, 4> additionalBindings;

    // INPUTS storage trio (storage_input SSBO / csf_image_input image2D /
    // uniform_input UBO) — order MUST match isf_emit_graphics_storage's
    // GLSL emission (declaration order, sequential bindings starting at
    // max_binding == 3 + samplers count).
    {
      auto extras = buildExtraBindings(m_storage);
      for(const auto& b : extras)
      {
        additionalBindings.push_back(b);
        max_binding++;
      }
    }

    for(auto& aux : m_auxiliarySSBOs)
    {
      // Dummy usage flag matches the aux kind so the created buffer can be
      // bound as the intended descriptor type (UBO for uniform_input, SSBO
      // otherwise). Mirrors the non-MRT path.
      if(!aux.buffer)
      {
        auto usage = aux.is_uniform ? QRhiBuffer::UniformBuffer
                                    : QRhiBuffer::StorageBuffer;
        const int64_t dummySize = aux.is_uniform ? 256 : 16;
        auto* dummy = rhi.newBuffer(QRhiBuffer::Immutable, usage, dummySize);
        dummy->setName(aux.is_uniform ? "RRP_ubo_dummy" : "RRP_aux_dummy");
        dummy->create();
        aux.buffer = dummy;
        aux.size = dummySize;
        aux.owned = true;
      }

      // Persistent ping-pong: <name>_prev (readonly) goes first.
      if(aux.persistent && aux.prev_buffer)
      {
        additionalBindings.push_back(
            QRhiShaderResourceBinding::bufferLoad(
                max_binding, bindingStages, aux.prev_buffer));
        aux.prev_binding = max_binding;
        max_binding++;
      }

      QRhiShaderResourceBinding binding;
      if(aux.is_uniform)
      {
        // uniform_input → std140 UBO binding
        binding = QRhiShaderResourceBinding::uniformBuffer(
            max_binding, bindingStages, aux.buffer);
      }
      else if(aux.access == "read_only")
        binding = QRhiShaderResourceBinding::bufferLoad(
            max_binding, bindingStages, aux.buffer);
      else if(aux.access == "write_only")
        binding = QRhiShaderResourceBinding::bufferStore(
            max_binding, bindingStages, aux.buffer);
      else
        binding = QRhiShaderResourceBinding::bufferLoadStore(
            max_binding, bindingStages, aux.buffer);

      additionalBindings.push_back(binding);
      aux.binding = max_binding;  // remember slot for per-sub-mesh patching
      max_binding++;
    }

    // Auxiliary texture / storage-image bindings (MRT path). Same
    // is_storage dispatch as the non-MRT site.
    for(auto& ats : m_auxTextureSamplers)
    {
      QRhiShaderResourceBinding b;
      if(ats.is_storage)
      {
        if(ats.access == "read_only")
          b = QRhiShaderResourceBinding::imageLoad(
              max_binding, bindingStages, ats.texture, 0);
        else if(ats.access == "write_only")
          b = QRhiShaderResourceBinding::imageStore(
              max_binding, bindingStages, ats.texture, 0);
        else
          b = QRhiShaderResourceBinding::imageLoadStore(
              max_binding, bindingStages, ats.texture, 0);
      }
      else
      {
        b = QRhiShaderResourceBinding::sampledTexture(
            max_binding, bindingStages, ats.texture, ats.sampler);
      }
      additionalBindings.push_back(b);
      ats.binding = max_binding;
      max_binding++;
    }

    additionalBindings.push_back(QRhiShaderResourceBinding::uniformBuffer(
        max_binding, bindingStages, m_modelUBO));

    auto bindings = createDefaultBindings(
        renderer, m_mrtRenderTarget, pubo, m_materialUBO, allSamplers(),
        std::span<QRhiShaderResourceBinding>(
            additionalBindings.data(), additionalBindings.size()));

    auto ps = rhi.newGraphicsPipeline();
    ps->setName("RenderedRawRasterPipelineNode::initMRTPass::ps");
    SCORE_ASSERT(ps);

    const int rtSamples = m_mrtRenderTarget.sampleCount();
    const int pipelineSamples = (rtSamples > 0) ? rtSamples : renderer.samples();
    ps->setSampleCount(pipelineSamples);

    // Multiview: activate the matching view count on the pipeline so that
    // `gl_ViewIndex` in the shader actually picks up the per-view state
    // (mat4[] viewProjection etc., emitted by the ISF layer). Must match
    // the color attachment's setMultiViewCount set in
    // createMultiViewRenderTarget above.
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    if(wantMultiview)
      ps->setMultiViewCount(mvCount);
#endif

    // preparePipeline sets the vertex-input layout from the mesh's
    // attributes. Skip for procedural draws (VERTEX_INPUTS: []): the
    // pipeline has no vertex bindings and the draw uses gl_VertexIndex.
    if(m_mesh)
      m_mesh->preparePipeline(*ps);

    const auto& desc = n.m_descriptor;
    const bool hasDescriptorState = stateAffectsPipeline(desc.default_state);

    if(hasDescriptorState)
    {
      // Seed legacy material-UBO blend on every attachment first; applyPipelineState
      // only overrides BLEND when the shader explicitly declares it.
      QRhiGraphicsPipeline::TargetBlend seededBlend;
      seededBlend.enable = mat.enable_blend;
      seededBlend.srcColor = mat.src_color;
      seededBlend.dstColor = mat.dst_color;
      seededBlend.opColor = mat.op_color;
      seededBlend.srcAlpha = mat.src_alpha;
      seededBlend.dstAlpha = mat.dst_alpha;
      seededBlend.opAlpha = mat.op_alpha;
      QList<QRhiGraphicsPipeline::TargetBlend> seedBlends;
      for(int i = 0; i < std::max(1, m_mrtRenderTarget.colorAttachmentCount()); i++)
        seedBlends.append(seededBlend);
      ps->setTargetBlends(seedBlends.begin(), seedBlends.end());
      ps->setDepthTest(true);
      ps->setDepthWrite(true);
      // Reverse-Z project rule (applyPipelineState overrides only if the
      // shader explicitly declares depth_compare).
      ps->setDepthOp(QRhiGraphicsPipeline::Greater);

      const bool depthAvailable
          = (m_mrtRenderTarget.depthTexture != nullptr)
            || (m_mrtRenderTarget.depthRenderBuffer != nullptr)
            || (m_mrtRenderTarget.msDepthTexture != nullptr);
      applyPipelineState(
          *ps, desc.default_state, m_mrtRenderTarget.colorAttachmentCount(),
          depthAvailable, /*wantsDepthByDefault=*/true);
    }
    else
    {
      // Legacy: material-UBO-driven blend, hardcoded depth.
      QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;
      premulAlphaBlend.enable = mat.enable_blend;
      premulAlphaBlend.srcColor = mat.src_color;
      premulAlphaBlend.dstColor = mat.dst_color;
      premulAlphaBlend.opColor = mat.op_color;
      premulAlphaBlend.srcAlpha = mat.src_alpha;
      premulAlphaBlend.dstAlpha = mat.dst_alpha;
      premulAlphaBlend.opAlpha = mat.op_alpha;

      QList<QRhiGraphicsPipeline::TargetBlend> blends;
      for(int i = 0; i < m_mrtRenderTarget.colorAttachmentCount(); i++)
        blends.append(premulAlphaBlend);
      ps->setTargetBlends(blends.begin(), blends.end());

      ps->setDepthTest(true);
      ps->setDepthWrite(true);
      // Reverse-Z project rule.
      ps->setDepthOp(QRhiGraphicsPipeline::Greater);
    }

    switch(mat.mode)
    {
      default:
      case 0:
        ps->setTopology(QRhiGraphicsPipeline::Triangles);
        break;
      case 1:
        ps->setTopology(QRhiGraphicsPipeline::Points);
        break;
      case 2:
        ps->setTopology(QRhiGraphicsPipeline::Lines);
        break;
    }

    // Remap vertex inputs by semantic (CSF-style; honour explicit
    // SEMANTIC). Procedural draws have no vertex inputs to remap — skip.
    // Same fallback-aware path as initPass — "REQUIRED: false" inputs
    // missing upstream land on a pooled identity buffer.
    FallbackBindingPlan fallbackPlan;
    if(m_mesh)
    {
      if(auto* geom = m_mesh->semanticGeometry())
      {
        if(!remapPipelineVertexInputs(
               *ps, v, *geom, n.descriptor(),
               rhi, renderer.vertexFallbackPool(), res, fallbackPlan))
        {
          qWarning() << "RawRaster::initMRTPass: remapPipelineVertexInputs FAILED";
          delete ps;
          delete pubo;
          return;
        }
      }
    }

    ps->setShaderStages({{QRhiShaderStage::Vertex, v}, {QRhiShaderStage::Fragment, s}});
    ps->setShaderResourceBindings(bindings);

    SCORE_ASSERT(m_mrtRenderTarget.renderPass);
    ps->setRenderPassDescriptor(m_mrtRenderTarget.renderPass);

    if(!ps->create())
    {
      qDebug() << "Warning! MRT Pipeline not created";
      delete ps;
      ps = nullptr;
    }

    Pipeline pip = {ps, bindings};
    if(pip.pipeline)
    {
      // nullptr edge — MRT passes are shared across all output edges
      Pass pass{m_mrtRenderTarget, pip, pubo};
      pass.fallback_bindings = std::move(fallbackPlan);
      m_passes.emplace_back(nullptr, std::move(pass));
    }
    else
    {
      delete pubo;
    }
  }
  catch(...)
  {
    delete pubo;
  }
}

void RenderedRawRasterPipelineNode::initMRTBlitPass(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge& edge)
{
  QRhiTexture* srcTex = textureForOutput(*edge.source);
  if(!srcTex)
    return;

  auto rt = renderer.renderTargetForOutput(edge);
  if(!rt.renderTarget)
    return;

  auto [vertexS, fragmentS] = score::gfx::makeShaders(renderer.state, rrp_blit_vs, rrp_blit_fs);

  QRhiSampler* sampler = renderer.state.rhi->newSampler(
      QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
  sampler->setName("RenderedRawRasterPipelineNode::MRT::blitSampler");
  sampler->create();
  m_blitSamplersByEdge[&edge] = sampler;

  auto pip = score::gfx::buildPipeline(
      renderer, *m_blitMesh, vertexS, fragmentS, rt, nullptr, nullptr,
      std::array<Sampler, 1>{Sampler{sampler, srcTex}});

  if(pip.pipeline)
  {
    m_passes.emplace_back(&edge, Pass{rt, pip, nullptr});
  }
  else
  {
    m_blitSamplersByEdge.erase(&edge);
    delete sampler;
  }
}

void RenderedRawRasterPipelineNode::initMRTBlitPasses(
    RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  // For each output port, create a blit pass for each downstream edge
  for(auto* output_port : n.output)
  {
    for(Edge* edge : output_port->edges)
    {
      initMRTBlitPass(renderer, res, *edge);
    }
  }
}

void RenderedRawRasterPipelineNode::initState(
    RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;

  // Create the mesh
  {
    if(geometry.meshes)
    {
      std::tie(m_mesh, m_meshbufs)
          = renderer.acquireMesh(geometry, res, m_mesh, m_meshbufs);
      m_meshbufs.gpuIndirectSupported = renderer.state.caps.drawIndirect;
    }
    else
    {
      if(m_mesh)
      {
        if(m_meshbufs.buffers.empty())
        {
          m_meshbufs = renderer.initMeshBuffer(*m_mesh, res);
          m_meshbufs.gpuIndirectSupported = renderer.state.caps.drawIndirect;
        }
      }
    }
  }

  // Create the material UBO
  m_materialSize = n.m_materialSize;
  if(m_materialSize > 0)
  {
    m_materialUBO
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
    m_materialUBO->setName("RenderedRawRasterPipelineNode::init::m_materialUBO");
    SCORE_ASSERT(m_materialUBO->create());
    if(n.m_material_data)
      res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, n.m_material_data.get());
  }

  m_modelUBO
      = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(float[16]));
  m_modelUBO->setName("RenderedRawRasterPipelineNode::init::m_modelUBO");
  SCORE_ASSERT(m_modelUBO->create());

  // Create the samplers
  SCORE_ASSERT(m_passes.empty());
  SCORE_ASSERT(m_inputSamplers.empty());
  SCORE_ASSERT(m_audioSamplers.empty());

  m_inputSamplers = initInputSamplers(this->n, renderer, n.input, &n.descriptor());

  // Build the auxiliary-texture binding table and seed initial texture
  // pointers from the incoming geometry. Walks desc.inputs parallel to
  // n.input and m_inputSamplers, recording a (sampler_idx, name) pair
  // for every image-style INPUT that might be served by a geometry aux
  // texture. update() re-runs the lookup whenever the geometry changes
  // so rebuilt / grown channel arrays flow through without a cable.
  bindAuxTexturesInit(renderer);

  m_audioSamplers = initAudioTextures(renderer, n.m_audio_textures);

  // Initialize auxiliary SSBOs from descriptor
  {
    const auto& desc = n.descriptor();
    m_auxiliarySSBOs.clear();
    m_auxiliarySSBOs.reserve(desc.auxiliary.size() + desc.inputs.size());

    // Resolve a buffer for `ssbo` by looking up its name in the first
    // incoming geometry's auxiliary_buffer list. Used for the scene-aware
    // wiring where the upstream ScenePreprocessor publishes scene_lights /
    // scene_materials / per_draw as named aux buffers travelling with the
    // geometry edge.
    auto try_bind_from_geometry = [&](AuxiliarySSBO& ssbo) {
      if(!geometry.meshes || geometry.meshes->meshes.empty())
        return;
      const auto& mesh = geometry.meshes->meshes[0];
      auto* geo_aux = mesh.find_auxiliary(ssbo.name);
      if(!geo_aux || geo_aux->buffer < 0
         || geo_aux->buffer >= (int)mesh.buffers.size())
        return;
      const auto& geo_buf = mesh.buffers[geo_aux->buffer];
      if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&geo_buf.data))
      {
        if(!gpu->handle)
          return;
        ssbo.buffer = static_cast<QRhiBuffer*>(gpu->handle);
        ssbo.size = geo_aux->byte_size > 0 ? geo_aux->byte_size : gpu->byte_size;
        ssbo.owned = false;
      }
      else if(auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&geo_buf.data))
      {
        if(!cpu->raw_data || cpu->byte_size <= 0)
          return;
        int64_t sz = geo_aux->byte_size > 0 ? geo_aux->byte_size : cpu->byte_size;
        // Usage flag must match the aux kind — binding a StorageBuffer-
        // only buffer as a uniform block (or vice versa) is rejected by
        // the Vulkan validation layer.
        const auto usage = ssbo.is_uniform ? QRhiBuffer::UniformBuffer
                                           : QRhiBuffer::StorageBuffer;
        auto* buf = rhi.newBuffer(QRhiBuffer::Immutable, usage, sz);
        buf->setName(QByteArray("RRP_aux_") + ssbo.name.c_str());
        buf->create();
        res.uploadStaticBuffer(buf, 0, sz, cpu->raw_data.get());
        ssbo.buffer = buf;
        ssbo.size = sz;
        ssbo.owned = true;
      }
    };

    // Resolve a buffer for `ssbo` by scanning the connected input port's
    // edges for an upstream producer (CSF storage output, ExtractBuffer2,
    // ScenePreprocessor aux extractors, ...). Upstream renderers publish
    // their output buffer through the virtual NodeRenderer::bufferForOutput()
    // — Port::value is never written for buffer-typed outputs — so the
    // retrieval goes through RenderList::bufferForInput(edge).
    //
    // Complements try_bind_from_geometry: an INPUTS-declared storage_input/
    // uniform_input may be wired through a dedicated Buffer edge instead of
    // riding along with the geometry. Mirrors
    // IsfBindingsBuilder::bindUpstreamBuffers, which SimpleRenderedISFNode
    // uses for non-RawRaster shaders.
    auto try_bind_from_input_port = [&](AuxiliarySSBO& ssbo) {
      if(ssbo.input_port_index < 0
         || ssbo.input_port_index >= (int)n.input.size())
        return;
      Port* port = n.input[ssbo.input_port_index];
      if(!port || port->type != Types::Buffer)
        return;
      for(Edge* edge : port->edges)
      {
        if(!edge || !edge->source)
          continue;
        if(edge->source->type != Types::Buffer)
          continue;
        auto view = renderer.bufferForInput(*edge);
        if(!view.handle)
          continue;
        ssbo.buffer = view.handle;
        if(ssbo.size <= 0)
          ssbo.size = view.handle->size();
        ssbo.owned = false;
        break;
      }
    };

    // Compute the byte size required by a LAYOUT. Used when we need to
    // own the buffer (persistent aux). Flexible array members use `size`
    // as the element count (falls back to 1 if unspecified).
    auto aux_owned_size = [](const isf::geometry_input::auxiliary_request& aux) -> int64_t {
      int64_t total = 0;
      int64_t arr_elem_bytes = 0;
      for(const auto& f : aux.layout)
      {
        auto bracket = f.type.find('[');
        std::string base = (bracket == std::string::npos) ? f.type : f.type.substr(0, bracket);
        int64_t sz = 0;
        if(base == "float" || base == "int" || base == "uint") sz = 4;
        else if(base == "vec2" || base == "ivec2" || base == "uvec2") sz = 8;
        else if(base == "vec3" || base == "ivec3" || base == "uvec3") sz = 16; // std430 pads
        else if(base == "vec4" || base == "ivec4" || base == "uvec4") sz = 16;
        else if(base == "mat4") sz = 64;
        else if(base == "mat3") sz = 48;
        else sz = 16; // conservative default for unknown types / structs
        if(bracket != std::string::npos)
        {
          // Flexible array (`name[]`) — size comes from SIZE expression.
          arr_elem_bytes = sz;
        }
        else
        {
          total += sz;
        }
      }
      int64_t count = 1;
      if(!aux.size.empty())
      {
        try { count = std::max<int64_t>(1, std::stoll(aux.size)); }
        catch(const std::exception& e) {
          count = 1024; // TODO: evaluate $USER when we add it
          qWarning() << "RenderedRawRasterPipelineNode: aux SSBO size"
                     << aux.size.c_str() << "could not be parsed (" << e.what()
                     << "); falling back to 1024.";
        }
      }
      else if(arr_elem_bytes > 0)
      {
        qWarning() << "RenderedRawRasterPipelineNode: aux SSBO has element size but no count;"
                      " falling back to 1024.";
        count = 1024;
      }
      return total + arr_elem_bytes * count;
    };

    // Top-level AUXILIARY textures: allocate one QRhiSampler per sampled
    // entry (storage-image entries don't need a sampler — imageLoad /
    // imageStore don't take one), seed with a type-appropriate
    // placeholder texture. Actual upstream resolution happens in
    // rebindAuxTextures() every frame.
    for(const auto& atx : desc.auxiliary_textures)
    {
      AuxTextureAuxSampler ats;
      ats.name = atx.name;
      ats.is_storage = atx.is_storage;
      ats.access = atx.access;

      if(!atx.is_storage)
      {
        ats.sampler = score::gfx::makeSampler(rhi, atx.sampler);
        ats.sampler->setName(
            ("RRP_aux_tex_sampler::" + atx.name).c_str());
      }

      // Pick placeholder matching the declared shape. Stored separately
      // so rebindAuxTextures can revert to it when upstream stops
      // publishing the aux name (otherwise we'd keep the stale upstream
      // handle around — UAF waiting to happen when the producer releases
      // the texture).
      if(atx.is_cubemap)
        ats.placeholder = &renderer.emptyTextureCube();
      else if(atx.dimensions == 3)
        ats.placeholder = &renderer.emptyTexture3D();
      else if(atx.is_array)
        ats.placeholder = &renderer.emptyTextureArray();
      else
        ats.placeholder = &renderer.emptyTexture();
      ats.texture = ats.placeholder;

      m_auxTextureSamplers.push_back(std::move(ats));
    }

    // INPUTS storage_input / uniform_input: these have a matching score
    // input port created by ISFNode's isf_input_port_vis. We record its
    // index so update() can re-pull the upstream buffer if it changes
    // (useful when the upstream node's init() runs after ours and only
    // publishes its Port::value then).
    //
    // walk_descriptor_inputs() advances the cumulative port_counts in
    // lockstep with isf_input_port_vis (single source of truth — see
    // ISFVisitors.hpp). For RawRaster the cursor starts at 1 because
    // port 0 is the mandatory Geometry input.
    //
    // Ordering: GLSL emits desc.inputs first then top-level AUXILIARY,
    // so we push AuxiliarySSBOs in the same order — reversing would
    // shift every binding index by desc.auxiliary.size() and Vulkan
    // would reject the pipeline with "VkDescriptorType mismatch".
    const bool isRawRaster = (desc.mode == isf::descriptor::RawRaster);
    const port_counts startPC{isRawRaster ? 1 : 0, 0, 0};
    // INPUTS storage_input / csf_image_input / uniform_input are handled by
    // IsfBindingsBuilder's m_storage path (allocateStorageResources +
    // buildExtraBindings) so the SRB binding type matches what
    // isf_emit_graphics_storage emits in GLSL. See `isf.cpp:4073` for the
    // GLSL emission and `IsfBindingsBuilder.cpp:417` for the allocation
    // path. The previous hand-rolled walker here only handled storage_input
    // and uniform_input, silently skipping csf_image_input — the shader
    // would emit `image2D NAME at binding=N` while no descriptor was added,
    // triggering VUID-VkGraphicsPipelineCreateInfo-layout-07990 on bind.
    //
    // No-op for INPUTS storage/uniform/csf_image entries — IsfBindingsBuilder
    // handles them. We still need the walker for indirect_draw storage_input
    // (special-cased at runtime, no SRB binding).
    walk_descriptor_inputs(
        desc, startPC,
        [&](const isf::input& inp, const port_counts&, const port_counts&) {
          if(auto* s = ossia::get_if<isf::storage_input>(&inp.data))
          {
            if(!s->buffer_usage.empty())
              return; // indirect_draw handled elsewhere
          }
          // INPUTS storage_input / uniform_input / csf_image_input now flow
          // through m_storage (initialised below). All other variants:
          // nothing to record here; the canonical walker still advances
          // port_idx correctly via `delta`.
        });

    // Now init m_storage from desc.inputs (storage_input + csf_image_input
    // + uniform_input). Bindings start at 3 + samplers count to align with
    // the GLSL emission order (samplers first in the binding range, then
    // INPUTS storage in declaration order via isf_emit_graphics_storage,
    // then AUXILIARY storage, then AUXILIARY textures, then model UBO).
    if(m_firstStorageBinding < 0)
    {
      const int firstStorageBinding
          = 3 + (int)m_inputSamplers.size() + (int)m_audioSamplers.size();
      m_firstStorageBinding = firstStorageBinding;
      collectGraphicsStorageResources(desc, firstStorageBinding, m_storage);
    }
    ensureStorageResources(
        *renderer.state.rhi, res, renderer, desc, m_storage,
        renderer.state.renderSize);
    bindUpstreamBuffers(renderer, n.input, m_storage);
    // Read-only csf_image_input adopts the matching upstream
    // auxiliary_texture by name (the storage image an upstream CSF /
    // RawRaster published into its out_geo). The auto-allocated
    // placeholder is freed inside the helper. The SRB doesn't exist
    // yet at init time — patched in update() once the pass is built.
    // INPUTS storage_input / uniform_input also name-match against the
    // upstream geometry's auxiliary_buffers list — that's how
    // ScenePreprocessor publishes scene_lights / world_transforms /
    // per_draws / scene_materials / scene_counts / scene_light_indices /
    // camera UBO / env UBO into flattened-scene shaders (classic_pbr et al.).
    if(geometry.meshes && !geometry.meshes->meshes.empty())
    {
      bindUpstreamImagesFromGeometry(m_storage, geometry.meshes->meshes[0]);
      bindUpstreamBuffersFromGeometry(
          *renderer.state.rhi, res, m_storage, geometry.meshes->meshes[0]);
    }

    // Top-level AUXILIARY entries: no corresponding score input port —
    // resolved by name from the upstream geometry's auxiliary list.
    // Kind dispatch (is_uniform): SSBO → std430 buffer, UBO → std140
    // uniform. The AuxiliarySSBO struct already carries an is_uniform
    // flag that downstream allocation / SRB-build sites dispatch on.
    // Non-persistent: resolved from the incoming geometry.
    // Persistent: node owns a ping-pong pair (SSBO only — UBO + persistent
    // is a no-op per the parser's semantic note; this branch is gated on
    // !is_uniform).
    //
    // Ordering: GLSL emits these AFTER all INPUTS bindings, so we push
    // them after the INPUTS loop above to keep binding slots aligned
    // between shader and SRB.
    for(const auto& aux : desc.auxiliary)
    {
      AuxiliarySSBO ssbo;
      ssbo.name = aux.name;
      ssbo.access = aux.access;
      ssbo.persistent = aux.persistent && !aux.is_uniform;
      ssbo.is_uniform = aux.is_uniform;

      if(ssbo.persistent)
      {
        const int64_t sz = std::max<int64_t>(16, aux_owned_size(aux));
        auto alloc = [&](const char* suffix) -> QRhiBuffer* {
          auto* b = rhi.newBuffer(
              QRhiBuffer::Static, QRhiBuffer::StorageBuffer, (quint32)sz);
          b->setName(QByteArray("RRP_persistent_aux_") + aux.name.c_str() + suffix);
          b->create();
          // Zero-initialise so the first frame's readonly _prev reads don't
          // hit uninitialised memory.
          std::vector<char> zeros(sz, 0);
          res.uploadStaticBuffer(b, 0, sz, zeros.data());
          return b;
        };
        ssbo.buffer = alloc("");
        ssbo.prev_buffer = alloc("_prev");
        ssbo.size = sz;
        ssbo.owned = true;
      }
      else
      {
        try_bind_from_geometry(ssbo);
      }

      m_auxiliarySSBOs.push_back(std::move(ssbo));
    }
  }

  // Determine if we need MRT. MRT is required for anything that
  // `initMRTPass` knows how to allocate which the non-MRT single-
  // target path can't express: multiple colour attachments, explicit
  // depth output, layered / cubemap output, or multiview. Multiview
  // specifically needs the MRT path because the RT has a different
  // shape from a swap-chain RT.
  {
    const auto& outputs = n.descriptor().outputs;
    int colorCount = 0;
    bool hasDepth = false;
    bool hasLayered = false;
    bool hasCubemap = false;
    for(const auto& out : outputs)
    {
      if(out.type == "depth")
        hasDepth = true;
      else
        ++colorCount;
      if(out.layers > 1)
        hasLayered = true;
      if(out.is_cubemap)
        hasCubemap = true;
    }
    m_hasMRT = colorCount > 1 || hasDepth || hasLayered || hasCubemap
               || n.descriptor().multiview_count >= 2;
  }

  if(m_hasMRT)
  {
    // Initialize the blit mesh (default quad)
    m_blitMesh = &renderer.defaultQuad();
    if(m_blitMeshbufs.buffers.empty())
      m_blitMeshbufs = renderer.initMeshBuffer(*m_blitMesh, res);
  }

  m_initialized = true;
}

void RenderedRawRasterPipelineNode::addOutputPass(
    RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res)
{
  // Procedural draws (VERTEX_INPUTS: [] + VERTEX_COUNT) have no
  // upstream geometry; m_mesh stays null and the draw call doesn't
  // fetch vertex attributes. Don't block MRT setup on the absence
  // of a mesh in that case.
  if(!m_mesh && !isProceduralDraw())
    return;

  if(m_hasMRT)
  {
    // Create the shared MRT internal render target on first output edge
    if(m_mrtRenderTarget.texture == nullptr)
    {
      initMRTPass(renderer, res);
    }

    // Create the blit pass for this single edge
    initMRTBlitPass(renderer, res, edge);
  }
  else
  {
    auto rt = renderer.renderTargetForOutput(edge);
    if(rt.renderTarget)
    {
      initPass(rt, renderer, res, edge);
    }
  }
}

void RenderedRawRasterPipelineNode::removeOutputPass(RenderList& renderer, Edge& edge)
{
  // Find and erase the pass for this edge
  auto it = ossia::find_if(m_passes, [&](auto& p) { return p.first == &edge; });
  if(it != m_passes.end())
  {
    it->second.p.release();
    if(it->second.processUBO)
      it->second.processUBO->deleteLater();
    m_passes.erase(it);
  }

  if(m_hasMRT)
  {
    // Release the blit sampler for this edge
    auto sit = m_blitSamplersByEdge.find(&edge);
    if(sit != m_blitSamplersByEdge.end())
    {
      delete sit->second;
      m_blitSamplersByEdge.erase(sit);
    }

    // If no more blit passes remain (only the shared MRT pass with nullptr edge),
    // release MRT resources
    bool hasBlitPasses = false;
    for(auto& [e, pass] : m_passes)
    {
      if(e != nullptr)
      {
        hasBlitPasses = true;
        break;
      }
    }
    if(!hasBlitPasses)
    {
      // Remove the shared MRT pass
      auto mrtIt = ossia::find_if(m_passes, [](auto& p) { return p.first == nullptr; });
      if(mrtIt != m_passes.end())
      {
        mrtIt->second.p.release();
        if(mrtIt->second.processUBO)
          mrtIt->second.processUBO->deleteLater();
        m_passes.erase(mrtIt);
      }
      m_mrtRenderTarget.release();
    }
  }
}

bool RenderedRawRasterPipelineNode::hasOutputPassForEdge(Edge& edge) const
{
  return ossia::find_if(m_passes, [&](const auto& p) { return p.first == &edge; })
         != m_passes.end();
}

void RenderedRawRasterPipelineNode::releaseState(RenderList& r)
{
  if(!m_initialized)
    return;

  // Release all remaining passes
  {
    for(auto& texture : n.m_audio_textures)
    {
      auto it = texture.samplers.find(&r);
      if(it != texture.samplers.end())
      {
        if(auto tex = it->second.texture)
        {
          if(tex != &r.emptyTexture())
            tex->deleteLater();
        }
      }
    }

    for(auto& [edge, pass] : m_passes)
    {
      pass.p.release();

      if(pass.processUBO)
      {
        pass.processUBO->deleteLater();
      }
    }

    m_passes.clear();
  }

  for(auto sampler : m_inputSamplers)
  {
    delete sampler.sampler;
    // texture is deleted elsewhere
  }
  m_inputSamplers.clear();
  // Override entries are non-owning (registry-owned). Just drop the
  // pointers — the registry's destroy() will deleteLater the underlying
  // QRhiSampler.
  m_inputSamplerOverrides.clear();
  for(auto sampler : m_audioSamplers)
  {
    delete sampler.sampler;
    // texture is deleted elsewhere
  }
  m_audioSamplers.clear();
  for(auto& [edge, sampler] : m_blitSamplersByEdge)
  {
    delete sampler;
  }
  m_blitSamplersByEdge.clear();

  delete m_materialUBO;
  m_materialUBO = nullptr;

  delete m_modelUBO;
  m_modelUBO = nullptr;

  m_blitMeshbufs = {}; // Freed in RenderList

  for(auto& aux : m_auxiliarySSBOs)
  {
    if(aux.owned && aux.buffer)
      aux.buffer->deleteLater();
    if(aux.owned && aux.prev_buffer)
      aux.prev_buffer->deleteLater();
  }
  m_auxiliarySSBOs.clear();

  // INPUTS storage trio (storage_input/csf_image_input/uniform_input)
  // — owned by m_storage; release frees the underlying QRhiBuffer/Texture.
  m_storage.release();
  m_firstStorageBinding = -1;

  for(auto& ats : m_auxTextureSamplers)
  {
    if(ats.sampler)
      ats.sampler->deleteLater();
    // `texture` is either a renderer-owned placeholder or an upstream-
    // geometry-owned handle — we don't own it here.
  }
  m_auxTextureSamplers.clear();

  // Release per-mip / per-cube-face render targets. The underlying
  // colour texture is owned by m_mrtRenderTarget and freed via its
  // release() below — we only drop the per-iteration RT wrappers +
  // per-iteration depth textures that we alloc'd here.
  for(auto& e : m_mipRTs)
  {
    if(e.renderTarget)
      e.renderTarget->deleteLater();
    if(e.renderPass)
      e.renderPass->deleteLater();
    if(e.depth)
      e.depth->deleteLater();
  }
  m_mipRTs.clear();
  m_mipCount = 0;
  m_perMipOutputIndex = -1;
  m_perCubeFaceOutputIndex = -1;

  // PerLayer state — same shape as the init-time cleanup in update().
  // Color path is held in m_mipRTs (cleared above); depth path keeps
  // its scratch + shared RT outside m_mipRTs.
  if(m_perLayerSharedRT)
  {
    m_perLayerSharedRT->deleteLater();
    m_perLayerSharedRT = nullptr;
  }
  if(m_perLayerSharedRP)
  {
    m_perLayerSharedRP->deleteLater();
    m_perLayerSharedRP = nullptr;
  }
  if(m_perLayerScratchDepth)
  {
    m_perLayerScratchDepth->deleteLater();
    m_perLayerScratchDepth = nullptr;
  }
  if(m_perLayerDummyColor)
  {
    m_perLayerDummyColor->deleteLater();
    m_perLayerDummyColor = nullptr;
  }
  m_perLayerOutputDepthArray = nullptr;
  m_perLayerOutputIndex = -1;
  m_perLayerIsDepth = false;

  m_executionMode = ExecutionMode::Single;

  // CUBEMAP + MULTIVIEW shim textures. The shadow TextureArray is
  // slotted into m_mrtRenderTarget's colour attachment slot, so
  // m_mrtRenderTarget.release() below handles it. The cube, however,
  // lives outside m_mrtRenderTarget (it's the public output handle)
  // and must be deleteLater'd here.
  if(m_cubeCopyCube)
  {
    m_cubeCopyCube->deleteLater();
    m_cubeCopyCube = nullptr;
  }
  m_cubeCopyShadowArray = nullptr;  // owned via m_mrtRenderTarget
  m_cubeCopyOutputIdx = -1;

  // Per-invocation UBO + SRB pool (PerMip / PerCubeFace / Manual).
  for(auto* ubo : m_perInvocationUBOs)
    if(ubo) ubo->deleteLater();
  m_perInvocationUBOs.clear();
  for(auto* srb : m_perInvocationSRBs)
    if(srb) srb->deleteLater();
  m_perInvocationSRBs.clear();

  // Release MRT render target (textures are owned by us)
  if(m_hasMRT)
  {
    m_mrtRenderTarget.release();
    m_hasMRT = false;
  }

  m_mesh = nullptr;
  m_meshbufs = {};
  m_blitMesh = nullptr;

  m_initialized = false;
}

void RenderedRawRasterPipelineNode::addInputEdge(
    RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res)
{
  if(edge.sink->type == Types::Image)
  {
    // Find upstream texture
    if(auto it = edge.source->node->renderedNodes.find(&renderer);
       it != edge.source->node->renderedNodes.end())
    {
      if(auto* tex = it->second->textureForOutput(*edge.source))
      {
        auto rt = renderer.renderTargetForInputPort(*edge.sink);
        updateInputTexture(*edge.sink, tex, rt.depthTexture);
      }
    }
  }
}

void RenderedRawRasterPipelineNode::removeInputEdge(RenderList& renderer, Edge& edge)
{
  if(edge.sink->type == Types::Image)
  {
    // See SimpleRenderedISFNode::removeInputEdge — same dangling-depth-
    // sampler issue applies here when DEPTH: true inputs get disconnected.
    const bool hasDepthCompanion
        = (edge.sink->flags & Flag::SamplableDepth) == Flag::SamplableDepth;
    QRhiTexture* depthFallback
        = hasDepthCompanion ? &renderer.emptyTexture() : nullptr;
    updateInputTexture(*edge.sink, &renderer.emptyTexture(), depthFallback);
  }
}

void RenderedRawRasterPipelineNode::init(
    RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  initState(renderer, res);

  // Procedural shaders (gl_VertexIndex + VERTEX_COUNT) don't need an
  // upstream geometry cable — still wire their output passes.
  if(!m_mesh && !isProceduralDraw())
    return;

  for(auto* out_port : n.output)
    for(auto* edge : out_port->edges)
      addOutputPass(renderer, *edge, res);
}

bool RenderedRawRasterPipelineNode::updateMaterials(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
{
  bool mustRecreatePasses = false;
  // Update audio textures
  if(!n.m_audio_textures.empty() && !m_audioTex)
  {
    m_audioTex.emplace();
  }

  bool audioChanged = false;
  std::size_t audio_idx = 0;
  for(auto& audio : n.m_audio_textures)
  {
    if(std::optional<Sampler> sampl
       = m_audioTex->updateAudioTexture(audio, renderer, n.m_material_data.get(), res))
    {
      // Texture changed -> material changed
      audioChanged = true;

      auto& [rhiSampler, tex, fb_] = *sampl;
      // Keep m_audioSamplers[i].texture in sync with the live GPU texture so
      // any later pipeline rebuild (rt_changed path in RenderList::render
      // calling removeOutputPass + addOutputPass) uses the live binding
      // instead of the placeholder empty texture.
      if(audio_idx < m_audioSamplers.size())
        m_audioSamplers[audio_idx].texture = tex;

      for(auto& [e, pass] : m_passes)
      {
        score::gfx::replaceTexture(
            *pass.p.srb, rhiSampler, tex ? tex : &renderer.emptyTexture());
      }
    }
    ++audio_idx;
  }

  // Update material
  if(m_materialUBO && m_materialSize > 0 && (materialChanged || audioChanged))
  {
    char* data = n.m_material_data.get();
    SCORE_ASSERT(m_materialSize >= size_of_pipeline_material);
    if(std::memcmp(data, this->m_prevPipelineChangingMaterial, size_of_pipeline_material)
       != 0)
    {
      mustRecreatePasses = true;
      std::copy_n(data, size_of_pipeline_material, this->m_prevPipelineChangingMaterial);
    }
    res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
  }
  materialChanged = false;
  return mustRecreatePasses;
}

void RenderedRawRasterPipelineNode::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
{
  // Update node materials. This must be before any initial return,
  // otherwise we miss the materialsChanged
  bool mustRecreatePasses = updateMaterials(renderer, res, edge);
  bool recreateDueToMaterial = mustRecreatePasses;

  // Refresh upstream-bound storage_input / uniform_input buffers from input
  // ports. The first pass will pick them up via the SRB; subsequent passes
  // need bindUpstreamBuffers to patch their SRBs in-place — handled per-pass
  // when m_passes is iterated for SRB updates further down. (Safe to call
  // even with no SRB; the helper just refreshes the m_storage entries.)
  bindUpstreamBuffers(renderer, n.input, m_storage);
  // Same pattern for read-only csf_image_input: adopt the matching upstream
  // auxiliary_texture (a storage image written by an upstream CSF /
  // RawRaster). Called per-frame so a producer that switches its underlying
  // QRhiTexture on resize / rebuild flows through. The helper is
  // idempotent on the swap and unconditionally patches each SRB it's
  // given — so calling it once per pass refreshes every SRB while only
  // doing the actual upstream lookup + swap on the first iteration.
  if(geometry.meshes && !geometry.meshes->meshes.empty())
  {
    // Per-pass refresh of name-matched-from-geometry bindings (SSBO/UBO/
    // storage_image). bindUpstream*FromGeometry are idempotent on the
    // swap and unconditionally patch each SRB they're given — so calling
    // each once per pass refreshes every SRB while doing the actual
    // upstream lookup + swap only on the first iteration that observed
    // a change.
    for(auto& [edge, pass] : m_passes)
    {
      if(pass.p.srb)
      {
        bindUpstreamImagesFromGeometry(
            m_storage, geometry.meshes->meshes[0], pass.p.srb);
        bindUpstreamBuffersFromGeometry(
            *renderer.state.rhi, res, m_storage,
            geometry.meshes->meshes[0], pass.p.srb);
      }
    }
  }

  // Update the geometry (sync with ModelDisplayNode)

  if(this->geometryChanged)
  {
    if(geometry.meshes)
    {
      const Mesh* prevMesh = m_mesh;
      std::tie(m_mesh, m_meshbufs)
          = renderer.acquireMesh(geometry, res, m_mesh, m_meshbufs);
      m_meshbufs.gpuIndirectSupported = renderer.state.caps.drawIndirect;

      this->meshChangedIndex = this->m_mesh->dirtyGeometryIndex;

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
      // Check for standalone indirect draw buffer from Buffer input ports
      if(!m_meshbufs.useIndirectDraw)
      {
        for(auto* port : n.input)
        {
          if(port->type == Types::Buffer && !port->edges.empty())
          {
            auto bv = renderer.bufferForInput(*port->edges.front());
            if(bv.usage == BufferView::Usage::IndirectDraw)
            {
              m_meshbufs.indirectDrawBuffer = bv.handle;
              m_meshbufs.useIndirectDraw = true;
              m_meshbufs.indirectDrawIndexed = false;
              break;
            }
            else if(bv.usage == BufferView::Usage::IndirectDrawIndexed)
            {
              m_meshbufs.indirectDrawBuffer = bv.handle;
              m_meshbufs.useIndirectDraw = true;
              m_meshbufs.indirectDrawIndexed = true;
              break;
            }
          }
        }
      }
#endif

      // Only recreate passes when the mesh object itself changed (different
      // vertex layout / topology). When the same mesh is reused with updated
      // buffer contents (e.g. feedback ping-pong), the existing pipeline is
      // still valid — acquireMesh already updated the buffers in place.
      if(m_mesh != prevMesh || m_passes.empty())
        mustRecreatePasses = true;
    }
    else
    {
      // Geometry removed — need to recreate
      mustRecreatePasses = true;
    }
    this->geometryChanged = false;

    // Re-resolve image-input samplers against the geometry's aux
    // textures. Growing a channel's texture array on ScenePreprocessor
    // republishes the geometry with a new QRhiTexture*; picking that up
    // here keeps the SRB bound to the live array instead of the deleted
    // one. A sampler change forces pass recreation so the SRB rebinds.
    if(rebindAuxTextures())
      mustRecreatePasses = true;

    // Re-match auxiliary SSBOs from updated geometry
    if(geometry.meshes && !geometry.meshes->meshes.empty())
    {
      const auto& mesh = geometry.meshes->meshes[0];
      for(auto& aux : m_auxiliarySSBOs)
      {
        if(auto* geo_aux = mesh.find_auxiliary(aux.name))
        {
          if(geo_aux->buffer >= 0 && geo_aux->buffer < (int)mesh.buffers.size())
          {
            const auto& geo_buf = mesh.buffers[geo_aux->buffer];
            if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&geo_buf.data))
            {
              if(gpu->handle)
              {
                auto* new_buf = static_cast<QRhiBuffer*>(gpu->handle);
                if(aux.buffer != new_buf)
                {
                  if(aux.owned && aux.buffer)
                    aux.buffer->deleteLater();
                  aux.buffer = new_buf;
                  aux.size = geo_aux->byte_size > 0 ? geo_aux->byte_size : gpu->byte_size;
                  aux.owned = false;
                  mustRecreatePasses = true;
                }
              }
            }
            else if(auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&geo_buf.data))
            {
              // CPU buffer: upload to GPU
              if(cpu->raw_data && cpu->byte_size > 0)
              {
                auto& rhi = *renderer.state.rhi;
                int64_t sz = geo_aux->byte_size > 0 ? geo_aux->byte_size : cpu->byte_size;

                if(aux.owned && aux.buffer)
                  renderer.releaseBuffer(aux.buffer);

                auto* buf = rhi.newBuffer(
                    QRhiBuffer::Immutable, QRhiBuffer::StorageBuffer, sz);
                buf->setName(QByteArray("RRP_aux_") + aux.name.c_str());
                buf->create();
                res.uploadStaticBuffer(buf, 0, sz, cpu->raw_data.get());

                aux.buffer = buf;
                aux.size = sz;
                aux.owned = true;
                mustRecreatePasses = true;
              }
            }
          }
        }
      }
    }
  }

  // Per-frame: re-pull upstream buffers wired through Buffer input ports
  // (camera UBO, ExtractBuffer2 SSBOs, ...). Cheap: one virtual call per
  // aux that has an input port index. Runs every frame because we cannot
  // guarantee the upstream publisher's init() ran before ours — its
  // bufferForOutput() may only return a non-null handle a frame later.
  for(auto& aux : m_auxiliarySSBOs)
  {
    if(aux.input_port_index < 0
       || aux.input_port_index >= (int)n.input.size())
      continue;
    Port* port = n.input[aux.input_port_index];
    if(!port || port->type != Types::Buffer)
      continue;

    QRhiBuffer* upstream = nullptr;
    for(Edge* edge : port->edges)
    {
      if(!edge || !edge->source)
        continue;
      if(edge->source->type != Types::Buffer)
        continue;
      if(auto view = renderer.bufferForInput(*edge); view.handle)
      {
        upstream = view.handle;
        break;
      }
    }
    if(!upstream || upstream == aux.buffer)
      continue;

    // Drop any placeholder / previously-owned buffer and adopt upstream.
    if(aux.owned && aux.buffer)
      aux.buffer->deleteLater();
    aux.buffer = upstream;
    aux.size = upstream->size();
    aux.owned = false;
    mustRecreatePasses = true;
  }

  bool recreateDueToGeometry = mustRecreatePasses && !recreateDueToMaterial;

  const bool procedural = isProceduralDraw();
  if(!m_mesh && !procedural)
  {
    return;
  }

  // FIXME is that neeeded?
  // FIXME also not handling geometry_filter dirty geom so far
  // Procedural draws never have a mesh — skip the dirty check.
  bool meshDirty = m_mesh && m_mesh->hasGeometryChanged(meshChangedIndex);
  if(meshDirty)
  {
    mustRecreatePasses = true;
  }

  if(mustRecreatePasses)
  {
    for(auto& pass : m_passes)
    {
      pass.second.p.release();
      if(pass.second.processUBO)
        pass.second.processUBO->deleteLater();
    }
    m_passes.clear();

    for(auto& [e, sampler] : m_blitSamplersByEdge)
      sampler->deleteLater();
    m_blitSamplersByEdge.clear();

    if(m_hasMRT)
    {
      // Release and recreate the internal MRT render target
      m_mrtRenderTarget.release();
      initMRTPass(renderer, res);
      initMRTBlitPasses(renderer, res);
    }
    else
    {
      for(Edge* edge : n.output[0]->edges)
      {
        auto rt = renderer.renderTargetForOutput(*edge);
        if(rt.renderTarget)
        {
          initPass(rt, renderer, res, *edge);
        }
      }
    }

    // After pass recreation, the freshly built SRBs reference the
    // CURRENT m_storage entries. For storage_input/uniform_input that
    // are name-matched against the upstream geometry's auxiliary_buffers
    // (the ScenePreprocessor publishing pattern: scene_lights /
    // world_transforms / per_draws / scene_materials / scene_counts /
    // scene_light_indices / camera UBO / env UBO), m_storage entries
    // may still hold the 16-byte zero placeholder ensureStorageResources
    // allocated for owned SSBOs — the per-pass refresh loop below
    // (lines ~2640+) is gated on m_passes non-empty. On a fresh
    // RenderList (resize / graph rebuild) the very first frame's
    // initState ran with m_passes empty, init early-returned without
    // building m_passes, then the per-pass refresh below was a no-op,
    // and now mustRecreatePasses just built passes against the
    // placeholder. Re-fire bindUpstream*FromGeometry on the freshly
    // built SRBs so they pick up the live geometry buffers / textures
    // immediately. Without this, classic_pbr's scene_counts.light_count
    // reads as 0 on the resize frame → light loop runs 0 times → no
    // specular until the next frame patches the SRB.
    if(geometry.meshes && !geometry.meshes->meshes.empty())
    {
      for(auto& [edge, pass] : m_passes)
      {
        if(pass.p.srb)
        {
          bindUpstreamImagesFromGeometry(
              m_storage, geometry.meshes->meshes[0], pass.p.srb);
          bindUpstreamBuffersFromGeometry(
              *renderer.state.rhi, res, m_storage,
              geometry.meshes->meshes[0], pass.p.srb);
        }
      }

      // Sampler refresh: FIX-C above only patches m_storage entries
      // (csf_image_input / storage_input / uniform_input). Plain
      // image_input INPUTS (sampler2DArray, sampler2D, sampler3D, etc.)
      // live in m_inputSamplers and are refreshed only by
      // rebindAuxTextures Path A — gated on `geometryChanged` and run
      // ONCE earlier in update() (line ~2462). If
      // `geometry.meshes` was null at THAT moment (or if a sibling
      // renderer republishes a fresh mesh_list AFTER that call) the
      // sampler binding stays at its empty-texture placeholder OR a
      // stale (deleteLater'd) upstream pointer.
      //
      // For the textured-PBR pipelines this manifests as:
      // baseColorArray sampler reads garbage / NaN → BRDF math
      // collapses → specular vanishes (ambient + base color factor +
      // emissive remain). Untextured classic_pbr has zero image_input
      // INPUTS so its m_inputSamplers is empty and the bug can't
      // trigger — exactly the user-reported asymmetry.
      //
      // Re-run rebindAuxTextures here (idempotent: short-circuits when
      // the slot's cached texture pointer matches the upstream's
      // current pointer). When it returns true, hot-patch the existing
      // SRBs in place via replaceTexture rather than going through
      // another full mustRecreatePasses cycle — the pipeline layout
      // is unchanged, only the texture pointer needs swapping.
      if(rebindAuxTextures())
      {
        // Match key for replaceTexture MUST be the sampler that's
        // actually in the SRB binding. allSamplers() (line ~155-170)
        // substitutes m_inputSamplerOverrides[i] for m_inputSamplers[i]
        // when ScenePreprocessor publishes a per-bucket sampler_handle
        // (e.g. baseColorArray gets the bucket's QRhiSampler so each
        // glTF/FBX material's wrap/filter survives). replaceTexture
        // matches by sampler-pointer (Utils.cpp:435); using the
        // ORIGINAL m_inputSamplers[i].sampler as the key when the SRB
        // has the OVERRIDE silently no-ops — so the texture refresh
        // never lands on textured-PBR pipelines that go through
        // ScenePreprocessor's per-bucket sampler overrides. That was
        // the residual lighting glitch on resize.
        const auto srb_key = [&](std::size_t i) -> QRhiSampler* {
          if(i < m_inputSamplerOverrides.size() && m_inputSamplerOverrides[i])
            return m_inputSamplerOverrides[i];
          return m_inputSamplers[i].sampler;
        };
        for(auto& [edge, pass] : m_passes)
        {
          if(!pass.p.srb)
            continue;
          for(std::size_t i = 0; i < m_inputSamplers.size(); ++i)
          {
            auto& s = m_inputSamplers[i];
            if(s.texture && s.sampler)
              score::gfx::replaceTexture(
                  *pass.p.srb, srb_key(i), s.texture);
          }
        }
        for(auto* invSrb : m_perInvocationSRBs)
        {
          if(!invSrb)
            continue;
          for(std::size_t i = 0; i < m_inputSamplers.size(); ++i)
          {
            auto& s = m_inputSamplers[i];
            if(s.texture && s.sampler)
              score::gfx::replaceTexture(
                  *invSrb, srb_key(i), s.texture);
          }
        }
      }
    }
  }

  m_mrtRenderedThisFrame = false;

  n.standardUBO.passIndex = 0;
  n.standardUBO.frameIndex++;
  auto sz = renderer.renderSize(edge);
  n.standardUBO.renderSize[0] = sz.width();
  n.standardUBO.renderSize[1] = sz.height();

  // Update all the process UBOs (blit passes have nullptr processUBO)
  for(auto& [e, pass] : m_passes)
  {
    if(!pass.processUBO)
      continue;
    res.updateDynamicBuffer(
        pass.processUBO, 0, sizeof(ProcessUBO), &this->n.standardUBO);
  }

  res.updateDynamicBuffer(m_modelUBO, 0, sizeof(float[16]), m_modelTransform.matrix);

  // Reset event ports now that the material UBO has captured their pulse
  // value via updateMaterials() above. If anything fired, set the shared
  // materialChanged flag so next frame's updateMaterials() uploads the
  // now-zero CPU memory instead of being gated out as unchanged.
  if(n.resetEventPortsAfterFrame())
    this->materialChanged = true;

  // Persistent AUXILIARY ping-pong: swap buffer/prev_buffer pointers, then
  // patch every pipeline's SRB so binding slots reference the post-swap
  // buffers. Done at the end of update() so the pass that renders this
  // frame already reads the previous frame's writes via `<name>_prev`.
  bool anyPersistentSwap = false;
  for(auto& aux : m_auxiliarySSBOs)
  {
    if(!aux.persistent || !aux.prev_buffer || n.standardUBO.frameIndex < 2u)
      continue;
    std::swap(aux.buffer, aux.prev_buffer);
    anyPersistentSwap = true;
  }
  if(anyPersistentSwap)
  {
    for(auto& [e, pass] : m_passes)
    {
      if(!pass.p.srb)
        continue;
      for(const auto& aux : m_auxiliarySSBOs)
      {
        if(!aux.persistent || aux.binding < 0 || aux.prev_binding < 0)
          continue;
        score::gfx::replaceBuffer(*pass.p.srb, aux.prev_binding, aux.prev_buffer);
        score::gfx::replaceBuffer(*pass.p.srb, aux.binding, aux.buffer);
      }
      // No trailing create() — replaceBuffer's updateResources() fast
      // path already refreshes the backend descriptor state.
    }
    // Per-invocation SRB pool (PerMip / PerCubeFace / Manual EXECUTION_MODELs)
    // shares the same persistent aux bindings as pass.p.srb. Without this
    // loop, invocation 0 reads post-swap data while invocations 1..N-1 read
    // the pre-swap (now `prev_buffer`-backed) buffers.
    for(auto* invSrb : m_perInvocationSRBs)
    {
      if(!invSrb)
        continue;
      for(const auto& aux : m_auxiliarySSBOs)
      {
        if(!aux.persistent || aux.binding < 0 || aux.prev_binding < 0)
          continue;
        score::gfx::replaceBuffer(*invSrb, aux.prev_binding, aux.prev_buffer);
        score::gfx::replaceBuffer(*invSrb, aux.binding, aux.buffer);
      }
    }
  }
}

void RenderedRawRasterPipelineNode::release(RenderList& r)
{
  releaseState(r);
}

void RenderedRawRasterPipelineNode::bindAuxTexturesInit(RenderList& /*renderer*/)
{
  m_auxTextureBindings.clear();
  const auto& desc = n.descriptor();

  // initInputSamplers walks n.input[] and pushes samplers for each
  // Types::Image port: 1 sampler, plus an extra "depth sampler" when the
  // port has SamplableDepth (set for image_input.depth=true on a
  // non-GrabsFromSource input). walk_descriptor_inputs gives us the
  // canonical sampler delta per input (see isf_input_port_count_vis),
  // so each image-like INPUT lands on its matching sampler slot.
  walk_descriptor_inputs(
      desc, [&](const isf::input& inp, const port_counts& cur, const port_counts& delta) {
        if(delta.samplers > 0)
          m_auxTextureBindings.push_back({cur.samplers, inp.name});
      });

  // Seed initial texture pointers from whatever geometry was already
  // published at init() time (typically none — the real lookup happens
  // on the first update()'s geometryChanged branch).
  rebindAuxTextures();
}

bool RenderedRawRasterPipelineNode::rebindAuxTextures()
{
  bool changed = false;
  if(!geometry.meshes || geometry.meshes->meshes.empty())
    return changed;
  const auto& mesh = geometry.meshes->meshes[0];

  // Path A: texture *overrides* on input-port-backed samplers (legacy
  // pattern: an INPUTS image whose name matches a geometry aux texture
  // gets its sampler's texture pointer swapped). When the geometry
  // also publishes a sampler_handle, swap that too — that's how
  // ScenePreprocessor's per-bucket samplers (per-glTF wrap/filter)
  // override the shader's static INPUTS sampler config.
  for(const auto& b : m_auxTextureBindings)
  {
    if(b.sampler_idx < 0 || b.sampler_idx >= (int)m_inputSamplers.size())
      continue;
    const auto* aux = mesh.find_auxiliary_texture(b.name);
    if(!aux)
      continue;
    auto* tex = static_cast<QRhiTexture*>(aux->native_handle);
    if(!tex)
      continue;
    auto& slot = m_inputSamplers[b.sampler_idx];
    if(slot.texture != tex)
    {
      slot.texture = tex;
      changed = true;
    }
    // Sampler override is non-owning — the bucket (in GpuResourceRegistry)
    // owns the QRhiSampler. Stored in the parallel m_inputSamplerOverrides
    // vector so the original initInputSamplers-owned sampler stays in
    // m_inputSamplers and `delete sampler.sampler` in release() doesn't
    // free the registry's sampler. allSamplers() applies the override
    // when building the SRB.
    if((int)m_inputSamplerOverrides.size() <= b.sampler_idx)
      m_inputSamplerOverrides.resize(b.sampler_idx + 1, nullptr);
    auto* smp = aux->sampler_handle
                    ? static_cast<QRhiSampler*>(aux->sampler_handle)
                    : nullptr;
    if(m_inputSamplerOverrides[b.sampler_idx] != smp)
    {
      m_inputSamplerOverrides[b.sampler_idx] = smp;
      changed = true;
    }
  }

  // Path B: top-level AUXILIARY textures (no input port). Resolve each
  // entry against the geometry's auxiliary_textures by name; fall back
  // to the shape-matched placeholder when nothing matches so we never
  // keep a stale upstream handle (protects against UAFs when a producer
  // disconnects or frees its texture).
  bool auxTexChanged = false;
  for(auto& ats : m_auxTextureSamplers)
  {
    const auto* aux = mesh.find_auxiliary_texture(ats.name);
    auto* tex = aux ? static_cast<QRhiTexture*>(aux->native_handle) : nullptr;
    if(!tex)
      tex = ats.placeholder; // revert to empty of the right kind
    if(!tex || tex == ats.texture)
      continue;
    ats.texture = tex;
    auxTexChanged = true;
  }
  if(auxTexChanged)
  {
    // Batched SRB rebuild: one destroy+setBindings+create per pass,
    // regardless of how many aux texture handles changed this frame.
    // The per-texture `replaceTexture(srb, binding, tex)` overload each
    // does its own destroy/setBindings/create, so looping it N times
    // would trigger N full SRB rebuilds per pass per frame whenever
    // textures change. Using the vector overload lets us batch into a
    // single rebuild cycle.
    auto rebuildSrb = [&](QRhiShaderResourceBindings* srb) {
      if(!srb)
        return;
      std::vector<QRhiShaderResourceBinding> tmp;
      tmp.assign(srb->cbeginBindings(), srb->cendBindings());
      for(const auto& ats : m_auxTextureSamplers)
      {
        if(ats.binding < 0 || !ats.texture)
          continue;
        score::gfx::replaceTexture(tmp, ats.binding, ats.texture);
      }
      srb->destroy();
      srb->setBindings(tmp.begin(), tmp.end());
      srb->create();
    };
    for(auto& [e, pass] : m_passes)
      rebuildSrb(pass.p.srb);
    // Per-invocation SRB pool (PerMip / PerCubeFace / Manual
    // EXECUTION_MODELs) — clones of pass.p.srb taken at construction
    // (see initPass / initMRTPass per-invocation push). Without this
    // mirror, invocation 0 (which renders through pass.p.srb) sees the
    // refreshed aux texture while invocations 1..N-1 keep sampling the
    // stale handle indefinitely. Same shape as the SSBO ping-pong fix
    // for m_perInvocationSRBs above (line ~2649) — symmetric, the bug
    // here was that the SSBO fix didn't propagate to aux-texture
    // rebinds.
    for(auto* invSrb : m_perInvocationSRBs)
      rebuildSrb(invSrb);
    changed = true;
  }

  return changed;
}

void RenderedRawRasterPipelineNode::runInitialPasses(
    RenderList& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& updateBatch,
    Edge& edge)
{
  // MDI readback fallback: when the backend doesn't support drawIndirect,
  // synchronously read back the GPU indirect buffer so the CPU draw loop
  // has the commands ready for this frame's draw call.
  if(m_meshbufs.useIndirectDraw
     && !m_meshbufs.gpuIndirectSupported
     && m_meshbufs.cpuDrawCommands.empty()
     && m_meshbufs.indirectDrawBuffer
     && m_meshbufs.indirectDrawBuffer->size() > 0)
  {
    QRhi& rhi = *renderer.state.rhi;
    auto* rb = rhi.nextResourceUpdateBatch();
    const quint32 bufSize = m_meshbufs.indirectDrawBuffer->size();
    m_meshbufs.readbackResult.completed = [this, bufSize]() {
      const auto& data = m_meshbufs.readbackResult.data;
      constexpr int cmdSize = 5 * sizeof(uint32_t);
      const int cmdCount = data.size() / cmdSize;
      m_meshbufs.cpuDrawCommands.clear();
      m_meshbufs.cpuDrawCommands.reserve(cmdCount);
      const auto* raw = reinterpret_cast<const uint32_t*>(data.constData());
      for(int c = 0; c < cmdCount; ++c)
      {
        const uint32_t* p = raw + c * 5;
        m_meshbufs.cpuDrawCommands.push_back({
            .index_or_vertex_count = p[0],
            .instance_count = p[1],
            .first_index_or_vertex = p[2],
            .base_vertex = static_cast<int32_t>(p[3]),
            .first_instance = p[4]});
      }
    };
    rb->readBackBuffer(m_meshbufs.indirectDrawBuffer, 0, bufSize, &m_meshbufs.readbackResult);
    cb.resourceUpdate(rb);
    rhi.finish();
  }

  if(!m_hasMRT || m_passes.empty())
    return;
  // Procedural draws don't require a mesh/vertex buffers — the draw
  // call uses gl_VertexIndex with no vertex bindings. Block only on
  // the non-procedural path.
  if(!isProceduralDraw() && (!m_mesh || m_meshbufs.buffers.empty()))
    return;

  // Only render once per frame even if multiple downstream nodes trigger us
  if(m_mrtRenderedThisFrame)
    return;
  m_mrtRenderedThisFrame = true;

  // MRT: render into our internal multi-attachment render target
  auto& pass = m_passes[0].second;

  SCORE_ASSERT(pass.renderTarget.renderTarget);
  SCORE_ASSERT(pass.p.pipeline);
  SCORE_ASSERT(pass.p.srb);

  // Invocation-count resolution. Single → 1, PerMip / PerCubeFace →
  // m_mipCount (reused to store either mip count or face count = 6),
  // Manual → evaluate the COUNT expression (falls back to 1 when the
  // expression is empty / unparseable). Runs every frame for Manual so
  // the count can track live input values; cached for PerMip /
  // PerCubeFace since the target shape is fixed at init.
  int invocationCount = 1;
  if(m_executionMode == ExecutionMode::PerMip
     || m_executionMode == ExecutionMode::PerCubeFace
     || m_executionMode == ExecutionMode::PerLayer)
  {
    invocationCount = std::max(1, m_mipCount);
  }
  else if(m_executionMode == ExecutionMode::Manual)
  {
    m_manualCount = resolveManualInvocationCount();
    invocationCount = std::max(1, m_manualCount);
  }

  auto* mainTex = pass.renderTarget.texture;
  // Depth-only shaders have no colour attachment so mainTex is null;
  // fall back to the depth attachment for the render-target size, then
  // to the renderer's render-size as a last resort. PER_LAYER+depth
  // specifically declares WIDTH/HEIGHT on its depth output (e.g.
  // 2048×2048 for shadow maps) and we want the viewport to honour that
  // rather than the window size.
  QRhiTexture* sizeTex = mainTex
                             ? mainTex
                             : pass.renderTarget.depthTexture;
  const QSize baseSize
      = sizeTex ? sizeTex->pixelSize() : renderer.state.renderSize;

  QRhi& rhi = *renderer.state.rhi;

  // Grow the per-invocation UBO+SRB pool if invocationCount exceeds
  // what we've already allocated. Each extra UBO gets its own dynamic
  // slot (no inter-invocation aliasing of the underlying buffer — the
  // QRhi Dynamic-UBO single-slot constraint is what made PASSINDEX
  // collapse to the last-written value before this). SRB i clones the
  // main SRB with the process-UBO binding swapped to UBO i.
  const int needed_extra = std::max(0, invocationCount - 1);
  while((int)m_perInvocationUBOs.size() < needed_extra)
  {
    const int k = (int)m_perInvocationUBOs.size() + 1;

    auto* ubo = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
    ubo->setName(
        ("RRPNode::MRT::perInvocationUBO::" + std::to_string(k)).c_str());
    ubo->create();
    m_perInvocationUBOs.push_back(ubo);

    // Clone the main SRB's bindings, swap binding=1 (the process UBO
    // per ISF convention — see isf.cpp's emitted `layout(std140,
    // binding = 1) uniform process_t`) to point at our new buffer.
    // The main pass's SRB is the layout-defining parent; new SRBs are
    // structurally identical and therefore compatible with the main
    // pipeline.
    std::vector<QRhiShaderResourceBinding> tmp;
    if(pass.p.srb)
      tmp.assign(pass.p.srb->cbeginBindings(), pass.p.srb->cendBindings());
    for(auto& b : tmp)
    {
      auto* d = reinterpret_cast<QRhiShaderResourceBinding::Data*>(&b);
      if(d->type == QRhiShaderResourceBinding::Type::UniformBuffer
         && d->binding == 1)
      {
        d->u.ubuf.buf = ubo;
      }
    }
    auto* srb = rhi.newShaderResourceBindings();
    srb->setName(
        ("RRPNode::MRT::perInvocationSRB::" + std::to_string(k)).c_str());
    srb->setBindings(tmp.begin(), tmp.end());
    srb->create();
    m_perInvocationSRBs.push_back(srb);
  }
  for(int i = 0; i < invocationCount; ++i)
  {
    // Stamp the per-invocation index into ProcessUBO. For PerMip this
    // doubles as the mip level; for Manual it's the 0-based loop index.
    // Each invocation writes to ITS OWN UBO (one allocated per slot
    // above) so Dynamic-UBO single-slot-per-frame doesn't collapse
    // every draw to the last-uploaded value.
    QRhiBuffer* invUBO
        = (i == 0) ? pass.processUBO : m_perInvocationUBOs[i - 1];
    QRhiShaderResourceBindings* invSRB
        = (i == 0) ? pass.p.srb : m_perInvocationSRBs[i - 1];

    auto* invBatch = (i == 0 && updateBatch)
                         ? updateBatch
                         : rhi.nextResourceUpdateBatch();
    this->n.standardUBO.passIndex = i;
    invBatch->updateDynamicBuffer(
        invUBO, 0, sizeof(ProcessUBO), &this->n.standardUBO);
    if(i == 0)
      updateBatch = nullptr;

    QRhiTextureRenderTarget* rtForPass
        = dynamic_cast<QRhiTextureRenderTarget*>(pass.renderTarget.renderTarget);
    QSize viewportSize = baseSize;
    if(m_executionMode == ExecutionMode::PerMip
       && i < (int)m_mipRTs.size() && m_mipRTs[i].renderTarget)
    {
      rtForPass = m_mipRTs[i].renderTarget;
      viewportSize = QSize(
          std::max(1, baseSize.width() >> i),
          std::max(1, baseSize.height() >> i));
    }
    else if(m_executionMode == ExecutionMode::PerCubeFace
            && i < (int)m_mipRTs.size() && m_mipRTs[i].renderTarget)
    {
      // Per-face cubemap RT. Face size = base (no per-face mipping in
      // this first cut); viewport stays at baseSize.
      rtForPass = m_mipRTs[i].renderTarget;
    }
    else if(m_executionMode == ExecutionMode::PerLayer)
    {
      // Color path: one RT per layer (stored in m_mipRTs, same shape as
      // PerCubeFace). Depth path: a single shared RT bound to the
      // scratch depth — we copy into the OUTPUT array layer-i after
      // endPass below, so the same RT is reused across iterations.
      if(m_perLayerIsDepth && m_perLayerSharedRT)
      {
        rtForPass = m_perLayerSharedRT;
      }
      else if(!m_perLayerIsDepth && i < (int)m_mipRTs.size()
              && m_mipRTs[i].renderTarget)
      {
        rtForPass = m_mipRTs[i].renderTarget;
      }
    }

    cb.beginPass(rtForPass, Qt::transparent, {0.0f, 0}, invBatch);

    cb.setGraphicsPipeline(pass.p.pipeline);
    cb.setViewport(
        QRhiViewport(0, 0, viewportSize.width(), viewportSize.height()));

    // drawWithPerMeshAuxRebind sets shader resources and issues the
    // draw call (or the per-sub-mesh loop for multi-mesh inputs).
    // Pass the per-invocation SRB so each draw reads its own UBO.
    // Forward the pass's fallback-binding plan so "REQUIRED: false"
    // VERTEX_INPUTS get their identity buffers bound.
    drawWithPerMeshAuxRebind(
        *invSRB, cb,
        std::span<const FallbackBindingPlan::Slot>{
            pass.fallback_bindings.slots});

    cb.endPass();

    // PerLayer + depth: copy the just-rendered scratch into layer i of
    // the OUTPUT depth array. Qt RHI 6.11 has no per-layer depth
    // attachment API, so this scratch+copy dance is the only way to
    // populate distinct depth-array layers in N sequential passes.
    // Single-format / single-size copy; QRhi handles the
    // depth-write→transfer-src and transfer-dst→depth-write barriers
    // around it automatically.
    if(m_executionMode == ExecutionMode::PerLayer && m_perLayerIsDepth
       && m_perLayerScratchDepth && m_perLayerOutputDepthArray)
    {
      auto* copyBatch = rhi.nextResourceUpdateBatch();
      QRhiTextureCopyDescription cdesc;
      cdesc.setPixelSize(viewportSize);
      cdesc.setSourceLayer(0);
      cdesc.setSourceLevel(0);
      cdesc.setSourceTopLeft(QPoint(0, 0));
      cdesc.setDestinationLayer(i);
      cdesc.setDestinationLevel(0);
      cdesc.setDestinationTopLeft(QPoint(0, 0));
      copyBatch->copyTexture(
          m_perLayerOutputDepthArray, m_perLayerScratchDepth, cdesc);
      cb.resourceUpdate(copyBatch);
    }
  }

  // Transparent CUBEMAP + MULTIVIEW finaliser. After all render passes
  // have ended, copy each layer of the shadow TextureArray into the
  // matching face of the public CubeMap. QRhi cube face layer order
  // is +X, -X, +Y, -Y, +Z, -Z — same ordering as our IBL shaders'
  // gl_ViewIndex, so layer i maps to face i 1:1.
  //
  // When PER_MIP is also active, both array and cube are MipMapped
  // and we loop across the full mip chain: N * 6 copyTexture calls
  // for N mips. Still basically free (pure GPU blit) — a 512² cube
  // with 10 mips is 60 ops taking microseconds.
  if(m_cubeCopyShadowArray && m_cubeCopyCube)
  {
    auto* copyBatch = rhi.nextResourceUpdateBatch();
    const QSize faceSize = m_cubeCopyCube->pixelSize();
    const int mipLevels
        = (m_executionMode == ExecutionMode::PerMip && m_mipCount > 0)
              ? m_mipCount
              : 1;
    for(int mip = 0; mip < mipLevels; ++mip)
    {
      const QSize mipSize(
          std::max(1, faceSize.width() >> mip),
          std::max(1, faceSize.height() >> mip));
      for(int face = 0; face < 6; ++face)
      {
        QRhiTextureCopyDescription desc;
        desc.setPixelSize(mipSize);
        desc.setSourceLayer(face);
        desc.setSourceLevel(mip);
        desc.setSourceTopLeft(QPoint(0, 0));
        desc.setDestinationLayer(face);
        desc.setDestinationLevel(mip);
        desc.setDestinationTopLeft(QPoint(0, 0));
        copyBatch->copyTexture(
            m_cubeCopyCube, m_cubeCopyShadowArray, desc);
      }
    }
    cb.resourceUpdate(copyBatch);
  }

  // GENERATE_MIPS: walk OUTPUTS and call generateMips() on every
  // declared target. For cube-copy outputs the generated-on texture
  // is the public cube (not the shadow array — downstream samples
  // the cube, and the shadow array may not even have the MipMapped
  // flag in non-PER_MIP cases). For all other outputs it's the
  // colour attachment we allocated in colorTextures[].
  //
  // Skip when PER_MIP is active on the SAME output: the render loop
  // has already authored distinct content per mip, and generateMips
  // would overwrite those sub-mips with averaged base-level data.
  {
    auto* mipBatch = rhi.nextResourceUpdateBatch();
    bool any = false;
    int colorIdx = 0;
    for(const auto& out : n.descriptor().outputs)
    {
      if(out.type == "depth")
        continue;
      if(out.generate_mips)
      {
        const bool perMipOwnsThis
            = m_executionMode == ExecutionMode::PerMip
              && colorIdx == m_perMipOutputIndex;
        if(!perMipOwnsThis)
        {
          QRhiTexture* tgt
              = (colorIdx == m_cubeCopyOutputIdx && m_cubeCopyCube)
                    ? m_cubeCopyCube
                    : (colorIdx == 0
                           ? pass.renderTarget.texture
                           : (colorIdx - 1
                                      < (int)pass.renderTarget
                                            .additionalColorTextures.size()
                                  ? pass.renderTarget
                                        .additionalColorTextures[colorIdx - 1]
                                  : nullptr));
          if(tgt)
          {
            mipBatch->generateMips(tgt);
            any = true;
          }
        }
      }
      ++colorIdx;
    }
    if(any)
      cb.resourceUpdate(mipBatch);
    else
      mipBatch->release();
  }
}

void RenderedRawRasterPipelineNode::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  // Plan 09 S6: debug marker for capture-tool readability (RenderDoc /
  // Nsight show the scope boundary + node name). No GPU timing
  // attribution here — QRhi's lastCompletedGpuTime is CB-scope, not
  // pass-scope. RAII via QByteArray lifetime keeps the end-marker
  // paired even on early returns.
  cb.debugMarkBegin(QByteArrayLiteral("RawRasterPipeline"));
  struct MarkEnd
  {
    QRhiCommandBuffer* c;
    ~MarkEnd() { c->debugMarkEnd(); }
  } _me{&cb};

  // MRT nodes render to their internal target in runInitialPasses,
  // then blit the appropriate texture here.
  if(m_hasMRT)
  {
    // Find the blit pass for this edge
    auto it = ossia::find_if(this->m_passes, [&](auto& p) { return p.first == &edge; });
    if(it == this->m_passes.end())
      return;

    auto& pass = it->second;
    SCORE_ASSERT(pass.renderTarget.renderTarget);
    SCORE_ASSERT(pass.p.pipeline);
    SCORE_ASSERT(pass.p.srb);

    cb.setGraphicsPipeline(pass.p.pipeline);
    cb.setShaderResources(pass.p.srb);

    auto* tex = pass.renderTarget.texture;
    cb.setViewport(QRhiViewport(
        0, 0, tex->pixelSize().width(), tex->pixelSize().height()));

    m_blitMesh->draw(this->m_blitMeshbufs, cb);
    return;
  }

  auto it = ossia::find_if(this->m_passes, [&](auto& p) { return p.first == &edge; });
  // Maybe the shader could not be created
  if(it == this->m_passes.end())
    return;
  // Procedural draws (VERTEX_INPUTS: [] + VERTEX_COUNT) have no mesh
  // and no vertex bindings — the draw issues cb.draw(vcount, icount)
  // directly via drawWithPerMeshAuxRebind's VERTEX_COUNT branch.
  const bool procedural = isProceduralDraw();
  if(!procedural && (!m_mesh || this->m_meshbufs.buffers.empty()))
    return;

  auto& pass = it->second;

  // Draw the last pass
  {
    SCORE_ASSERT(pass.renderTarget.renderTarget);
    SCORE_ASSERT(pass.p.pipeline);
    SCORE_ASSERT(pass.p.srb);

    auto pipeline = pass.p.pipeline;
    auto srb = pass.p.srb;
    auto texture = pass.renderTarget.texture;

    {
      cb.setGraphicsPipeline(pipeline);
      cb.setViewport(QRhiViewport(
          0, 0, texture->pixelSize().width(), texture->pixelSize().height()));

      drawWithPerMeshAuxRebind(
          *srb, cb,
          std::span<const FallbackBindingPlan::Slot>{
              pass.fallback_bindings.slots});
    }
  }
}

void RenderedRawRasterPipelineNode::process(int32_t port, const ossia::transform3d& v)
{
  m_modelTransform = v;
}

void RenderedRawRasterPipelineNode::drawWithPerMeshAuxRebind(
    QRhiShaderResourceBindings& srb, QRhiCommandBuffer& cb,
    std::span<const FallbackBindingPlan::Slot> fallback_slots)
{
  // Phase 2 unified MDI: ScenePreprocessor's output geometry is now
  // ALWAYS a single sub-mesh (regular meshes + instance groups all
  // ride through one drawIndexedIndirect / one cpu_draw_commands
  // iteration). There is no per-sub-mesh SRB rebind to do — the SRB
  // is bound once and the draw fans out via the indirect cmd list.
  // The legacy name is preserved for now to avoid churning every
  // call-site; rename pass deferred.
  cb.setShaderResources(&srb);

  // PIPELINE_STATE: { "VERTEX_COUNT": N, "INSTANCE_COUNT": M,
  // "TOPOLOGY": "..." } — procedural/VSA-style draw override. Issue a
  // single cb.draw(N, M, 0, 0) and ignore the incoming geometry's
  // index/indirect buffers entirely; the vertex shader drives positions
  // from gl_VertexIndex + gl_InstanceIndex. Used for fullscreen passes
  // (skybox: VERTEX_COUNT=3), procedural geometry (VSA plasma:
  // VERTEX_COUNT=10000, TOPOLOGY=line_strip), etc. Without this, a
  // fullscreen pass wired to a complex scene rasterizes N/3 fullscreen
  // triangles — devastating even with early-Z (SciFiHelmet → ~46k
  // fullscreen tris → ~100ms/frame on a GTX 1080).
  //
  // Safety: if the shader declares non-empty VERTEX_INPUTS (i.e. reads
  // vertex attributes), clamp the draw count to the incoming geometry's
  // vertex_count so the VS can't fetch past the bound buffer. Shaders
  // that live purely on gl_VertexIndex should declare `VERTEX_INPUTS:
  // []` — the pipeline is then built with no vertex bindings and
  // VERTEX_COUNT is used verbatim.
  {
    const auto& ds = n.descriptor().default_state;
    if(ds.vertex_count.has_value())
    {
      uint32_t vcount = *ds.vertex_count;
      const uint32_t icount = ds.instance_count.value_or(1u);

      const bool hasVertexInputs = !n.descriptor().vertex_inputs.empty();
      if(hasVertexInputs && this->geometry.meshes
         && !this->geometry.meshes->meshes.empty())
      {
        const uint32_t incoming
            = (uint32_t)this->geometry.meshes->meshes[0].vertices;
        if(incoming > 0 && vcount > incoming)
          vcount = incoming;
      }

      // Bind vertex buffers driven by the geometry's `input` list — NOT
      // every entry in m_meshbufs.buffers. Since the scene preprocessor
      // started appending the index buffer + scene-wide SSBOs (lights /
      // materials / per-draws / …) to g.buffers for the auxiliary
      // mapping, blindly binding the buffers array pushes STORAGE / INDEX
      // buffers into vertex binding slots and Vulkan validation fires
      // `VUID-vkCmdBindVertexBuffers-pBuffers-00627`. g.input is the
      // authoritative vertex-binding list.
      std::array<QRhiCommandBuffer::VertexInput, 8> inputs;
      std::size_t nb = 0;
      if(this->geometry.meshes && !this->geometry.meshes->meshes.empty())
      {
        const auto& g0 = this->geometry.meshes->meshes[0];
        const std::size_t cap = inputs.size();
        for(const auto& in : g0.input)
        {
          if(nb >= cap)
            break;
          const std::size_t idx = (std::size_t)in.buffer;
          if(idx >= m_meshbufs.buffers.size())
            continue;
          auto* h = m_meshbufs.buffers[idx].handle;
          if(!h)
            continue;
          inputs[nb++] = {h, (quint32)in.byte_offset};
        }
      }
      if(nb > 0)
        cb.setVertexInput(0, (int)nb, inputs.data());

      if(vcount > 0 && icount > 0)
        cb.draw(vcount, icount, 0, 0);
      return;
    }
  }

  // Single-mesh draw. ScenePreprocessor unified-MDI emits one sub-mesh
  // covering every regular cmd + every instance group; the indirect cmd
  // list fans out across them. Per-pass pipeline swapping (alpha-blend
  // etc.) is NOT handled here — that's the job of a dedicated
  // downstream node configured by the user as a separate render pass.
  if(m_mesh)
  {
    // Fallback-aware draw when the shader declared "REQUIRED: false"
    // VERTEX_INPUTS whose semantics are missing from upstream geometry.
    // Plain pass-through otherwise (zero overhead when the plan is empty).
    if(!fallback_slots.empty())
    {
      if(auto* cm2 = dynamic_cast<const CustomMesh*>(m_mesh))
        cm2->drawWithFallbackBindings(m_meshbufs, cb, fallback_slots);
      else
        m_mesh->draw(m_meshbufs, cb);
    }
    else
    {
      m_mesh->draw(m_meshbufs, cb);
    }
  }
}

RenderedRawRasterPipelineNode::~RenderedRawRasterPipelineNode() { }

bool RenderedRawRasterPipelineNode::isProceduralDraw() const noexcept
{
  const auto& desc = n.descriptor();
  return desc.vertex_inputs.empty()
         && desc.default_state.vertex_count.has_value()
         && *desc.default_state.vertex_count > 0;
}

// Generic integer-expression evaluator. Shared by EXECUTION_MODEL=MANUAL
// (COUNT) and OUTPUTS.WIDTH / HEIGHT. Pure-integer fast path avoids the
// expression parser for the overwhelmingly common literal case.
// Variable surface matches CSF dispatch expressions so all three sites
// share a mental model: $WIDTH / $HEIGHT / $DEPTH / $LAYERS of the first
// input image (unsuffixed + per-name variants), plus scalar input values
// as $<inputName>. '$' → 'var_' rewrite follows the CSF convention.
int RenderedRawRasterPipelineNode::resolveIntExpression(
    const std::string& expr, int fallback) const
{
  if(expr.empty())
    return fallback;

  // Pure-integer fast path — std::stoi would otherwise silently accept
  // "6 * $x" as 6 (ignoring the variable reference entirely).
  {
    std::size_t i = 0;
    while(i < expr.size() && std::isspace((unsigned char)expr[i]))
      ++i;
    const std::size_t first_digit = i;
    while(i < expr.size() && std::isdigit((unsigned char)expr[i]))
      ++i;
    const std::size_t last_digit = i;
    while(i < expr.size() && std::isspace((unsigned char)expr[i]))
      ++i;
    if(first_digit < last_digit && i == expr.size())
    {
      try
      {
        return std::max(1, std::stoi(expr));
      }
      catch(...)
      {
      }
    }
  }

  ossia::math_expression e;
  ossia::small_pod_vector<double, 16> data;
  data.reserve(16);

  auto register_size = [&](const std::string& name, QRhiTexture* tex,
                           bool& first) {
    QSize px = tex ? tex->pixelSize() : QSize{1280, 720};
    int depth = 1, layers = 1;
    if(tex)
    {
      if((int)(tex->flags() & QRhiTexture::ThreeDimensional))
        depth = std::max(1, tex->depth());
      if((int)(tex->flags() & QRhiTexture::TextureArray))
        layers = std::max(1, tex->arraySize());
    }
    if(px.width() <= 0)
      px.setWidth(1280);
    if(px.height() <= 0)
      px.setHeight(720);
    e.add_constant("var_WIDTH_" + name, data.emplace_back(px.width()));
    e.add_constant("var_HEIGHT_" + name, data.emplace_back(px.height()));
    e.add_constant("var_DEPTH_" + name, data.emplace_back(depth));
    e.add_constant("var_LAYERS_" + name, data.emplace_back(layers));
    if(first)
    {
      e.add_constant("var_WIDTH", data.emplace_back(px.width()));
      e.add_constant("var_HEIGHT", data.emplace_back(px.height()));
      e.add_constant("var_DEPTH", data.emplace_back(depth));
      e.add_constant("var_LAYERS", data.emplace_back(layers));
      first = false;
    }
  };

  // Walk the descriptor's image-style inputs in declared order so the
  // first one supplies the unsuffixed $WIDTH / $HEIGHT family, matching
  // CSF's `registerCommonExpressionVariables` semantics.
  bool first_image = true;
  int sampler_idx = 0;
  for(const auto& inp : n.descriptor().inputs)
  {
    if(ossia::get_if<isf::texture_input>(&inp.data)
       || ossia::get_if<isf::image_input>(&inp.data))
    {
      QRhiTexture* t = nullptr;
      if(sampler_idx < (int)m_inputSamplers.size())
        t = m_inputSamplers[sampler_idx].texture;
      register_size(inp.name, t, first_image);
      ++sampler_idx;
    }
  }

  // Scalar ports — mirror the $<inputName> surface. Walking node.input in
  // parallel with descriptor.inputs lets us pull live values without
  // reimplementing the port-dispatch plumbing.
  int port_idx = 0;
  for(const auto& inp : n.descriptor().inputs)
  {
    auto port = (port_idx < (int)n.input.size()) ? n.input[port_idx]
                                                 : nullptr;
    if(ossia::get_if<isf::float_input>(&inp.data))
    {
      if(port && port->value)
        e.add_constant(
            "var_" + inp.name, data.emplace_back(*(float*)port->value));
    }
    else if(ossia::get_if<isf::long_input>(&inp.data))
    {
      if(port && port->value)
        e.add_constant(
            "var_" + inp.name, data.emplace_back(*(int*)port->value));
    }
    ++port_idx;
  }

  // Register $COUNT_<bufferName> / $BYTESIZE_<bufferName> for every
  // SSBO / UBO the raster pipeline binds (INPUTS storage_input /
  // uniform_input, plus top-level AUXILIARY entries). Same semantics as
  // CSF: COUNT = element count of the flexible array (or 1 for UBOs /
  // fixed-layout SSBOs), BYTESIZE = raw byte size of the binding. Lets
  // OUTPUTS.WIDTH / HEIGHT / MANUAL-count expressions size themselves
  // against upstream buffer extents by name, matching the convention
  // used by CSF compute passes.
  //
  // Live sizes come from m_auxiliarySSBOs (populated at init time from
  // actual buffer allocations / upstream adoptions); layout comes from
  // the descriptor. Cross-reference by name.
  {
    ossia::hash_set<std::string> registered;
    const auto& desc = n.descriptor();

    // Find the live byte size for a given aux name. Falls back to 0 if
    // the binding isn't yet live (first frame, unbound edge, etc.) —
    // count then resolves to 1, which is the zero-copy-safe default.
    auto find_aux_size = [&](const std::string& name) -> int64_t {
      for(const auto& aux : m_auxiliarySSBOs)
        if(aux.name == name)
          return aux.size;
      return 0;
    };

    // Register a buffer whose storage-side layout is available. SSBOs
    // use the layout to derive element stride (fixed part + flexible-
    // array element), UBOs skip the layout lookup since they're always
    // one struct instance with $COUNT = 1.
    auto register_ssbo
        = [&](const std::string& name, int64_t byte_size,
              std::span<const isf::storage_input::layout_field> layout) {
      if(name.empty() || registered.contains(name))
        return;
      int64_t element_count = 1;
      const int64_t fixed_part
          = score::gfx::calculateStorageBufferSize(layout, 0, desc);
      const int64_t with_one
          = score::gfx::calculateStorageBufferSize(layout, 1, desc);
      const int64_t stride = with_one - fixed_part;
      if(stride > 0 && byte_size > fixed_part)
        element_count = (byte_size - fixed_part) / stride;
      if(element_count < 1)
        element_count = 1;
      e.add_constant(
          "var_COUNT_" + name, data.emplace_back((double)element_count));
      e.add_constant(
          "var_BYTESIZE_" + name, data.emplace_back((double)byte_size));
      registered.insert(name);
    };

    auto register_ubo
        = [&](const std::string& name, int64_t byte_size) {
      if(name.empty() || registered.contains(name))
        return;
      e.add_constant("var_COUNT_" + name, data.emplace_back(1.0));
      e.add_constant(
          "var_BYTESIZE_" + name, data.emplace_back((double)byte_size));
      registered.insert(name);
    };

    // INPUTS storage_input / uniform_input
    for(const auto& inp : desc.inputs)
    {
      if(auto* s = ossia::get_if<isf::storage_input>(&inp.data))
        register_ssbo(inp.name, find_aux_size(inp.name), s->layout);
      else if(ossia::get_if<isf::uniform_input>(&inp.data))
        register_ubo(inp.name, find_aux_size(inp.name));
    }

    // Top-level AUXILIARY entries (declared at descriptor root).
    for(const auto& aux : desc.auxiliary)
    {
      if(aux.is_uniform)
        register_ubo(aux.name, find_aux_size(aux.name));
      else
        register_ssbo(aux.name, find_aux_size(aux.name), aux.layout);
    }
  }

  std::string eval_expr = expr;
  boost::algorithm::replace_all(eval_expr, "$", "var_");
  e.register_symbol_table();
  if(e.set_expression(eval_expr))
    return std::max(1, (int)e.value());

  qWarning() << "RawRaster: integer expression failed:"
             << e.error().c_str() << eval_expr.c_str();
  return fallback;
}

int RenderedRawRasterPipelineNode::resolveManualInvocationCount() const
{
  return resolveIntExpression(
      n.descriptor().execution_model.count_expression, 1);
}

}
