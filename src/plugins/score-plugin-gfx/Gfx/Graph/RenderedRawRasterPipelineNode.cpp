#include <Gfx/Graph/CustomMesh.hpp>
#include <Gfx/Graph/ISFVisitors.hpp>
#include <Gfx/Graph/PipelineStateHelpers.hpp>
#include <Gfx/Graph/RhiClearBuffer.hpp>
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

#include <atomic>
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
#if !defined(QSHADER_SPIRV)
  // Everything except Vulkan, measured rather than derived -- the same shape,
  // and for the same reason, as ISF_STORE_COORD in libisf's computeMacros.
  // The framebuffer origin does not predict this: Direct3D and Metal put it at
  // the top like Vulkan, yet the copy from the intermediate MRT attachment to
  // the output render target reaches the delivered picture mirrored on them
  // exactly as it does on OpenGL. Gating on OpenGL alone
  // (QRhi::isYUpInFramebuffer()) leaves D3D11 and D3D12 upside down:
  // GfxRawRasterMrtPattern reports green=10 where 245 is expected at row 2, on
  // every attachment, on both D3D backends, while OpenGL and Vulkan are green.
  //
  // The direct (single-output) raw-raster path does not go through this blit
  // and is correctly oriented on all four backends, so the correction belongs
  // here and nowhere else. SimpleRenderedISFNode.cpp's twin blit is NOT the
  // same case: the ISF vertex prelude carries isf_vertShaderFinish, which
  // already flips for QSHADER_HLSL/QSHADER_MSL, and GfxMrtPattern -- the ISF
  // twin of this test, same closed form -- passes on both D3D backends.
  v_texcoord.y = 1. - v_texcoord.y;
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

// Layer 0 of an array source. A sampler2D bound to a VK_IMAGE_VIEW_TYPE_2D_ARRAY
// view is VUID-vkCmdDraw-viewType-07752.
static const constexpr auto rrp_blit_array_fs = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(binding = 3) uniform sampler2DArray blitTexture;
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main() { fragColor = texture(blitTexture, vec3(v_texcoord, 0.0)); }
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

  // The replaceTexture match key must be the sampler actually in the SRB
  // binding: allSamplers() substitutes m_inputSamplerOverrides[i] when a
  // per-bucket override is present. Same applies to the geometry-buffer
  // sampler rebind further down.
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
      // Patch the per-invocation SRB pool too (PER_LAYER / PER_MIP /
      // MANUAL COUNT>1 clone the main SRB). QRhi generation-tracking only
      // covers rebuilding the same object, and this swaps to a different
      // QRhiTexture*.
      for(auto* invSrb : m_perInvocationSRBs)
        if(invSrb)
          score::gfx::replaceTexture(*invSrb, key, tex);
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
        // Mirror onto the per-invocation SRB pool (see comment above).
        for(auto* invSrb : m_perInvocationSRBs)
          if(invSrb)
            score::gfx::replaceTexture(*invSrb, depthKey, depthTex);
      }
    }
  }
}

QRhiTexture* RenderedRawRasterPipelineNode::textureForOutput(const Port& output)
{
  if(!m_hasMRT)
    return nullptr;

  const auto& outputs = n.descriptor().outputs;
  for(int i = 0; i < (int)n.output.size() && i < (int)outputs.size(); i++)
  {
    if(n.output[i] == &output)
    {
      // Depth outputs expose the depth attachment directly: a multi-layer
      // Texture2DArray under EXECUTION_MODEL PER_LAYER, a plain 2D depth
      // texture otherwise. Downstream wires it through
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

  // Apply non-owning per-port sampler overrides published by the upstream
  // geometry's auxiliary_texture::sampler_handle. Applied only on the SRB-build
  // copy: m_inputSamplers keeps its own owning sampler so release() can delete
  // it without touching a registry-owned one.
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

// Diagnostic escape hatch, mirroring SCORE_GFX_NO_GPU_INDIRECT: set
// SCORE_GFX_NO_AUX_PLACEHOLDER_ZERO=1 to restore the pre-fix behaviour where an
// unbound AUXILIARY placeholder was created and never written. It exists so the
// crash this fix addresses can be A/B'd on the machine that reproduces it
// without a second build; nothing in score sets it.
static bool auxPlaceholderZeroFillDisabled() noexcept
{
  static const bool off
      = qEnvironmentVariableIntValue("SCORE_GFX_NO_AUX_PLACEHOLDER_ZERO") > 0;
  return off;
}

// Companion to traceAuxPlaceholder: logged from init(), before initPass runs,
// for EVERY declared AUXILIARY -- bound or not. An empty census means the node
// declares none and the placeholder trace below can never print.
static void traceAuxResolution(
    const std::string& name, bool bound, int64_t size) noexcept
{
  static const bool on
      = qEnvironmentVariableIntValue("SCORE_GFX_TRACE_AUX_PLACEHOLDER") > 0;
  if(!on)
    return;
  qDebug(
      "[AUX-RESOLVE] name=%s bound_from_geometry=%d bytes=%lld", name.c_str(),
      int(bound), (long long)size);
}

// SCORE_GFX_TRACE_AUX_PLACEHOLDER=1 logs every producerless AUXILIARY the node
// had to invent a buffer for. It is the positive control for the knob above: a
// run that prints no lines never allocated a placeholder, so toggling the
// zero-fill in that run proved nothing.
static void traceAuxPlaceholder(
    const std::string& name, int64_t size, bool uniform, bool zeroed) noexcept
{
  static const bool on
      = qEnvironmentVariableIntValue("SCORE_GFX_TRACE_AUX_PLACEHOLDER") > 0;
  if(!on)
    return;
  static std::atomic_int counter{0};
  qDebug(
      "[AUX-PLACEHOLDER #%d] name=%s kind=%s bytes=%lld zero_filled=%d",
      counter.fetch_add(1) + 1, name.c_str(), uniform ? "ubo" : "ssbo",
      (long long)size, int(zeroed));
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
    auto [v, s] = score::gfx::makeShaders(
        renderer.state, n.m_vertexS, n.m_fragmentS, n.descriptor().multiview_count);

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
      // If no buffer yet, create a dummy so the descriptor set is valid.
      // Dummy usage flag matches the aux kind so the created buffer can be
      // bound as the intended descriptor type. Sized from the shader's
      // LAYOUT (declared_size) — `aux.size` is 0 here, it is only ever
      // assigned where a buffer already exists.
      if(!aux.buffer)
      {
        auto usage = aux.is_uniform ? QRhiBuffer::UniformBuffer
                                    : QRhiBuffer::StorageBuffer;
        // Rounded up to 4: RhiClearBuffer's contract (vkCmdFillBuffer) wants a
        // 4-byte-aligned size.
        const int64_t dummySize
            = (std::max<int64_t>(aux.declared_size, aux.is_uniform ? 256 : 16) + 3)
              & ~int64_t(3);
        auto* dummy = rhi.newBuffer(bufferTypeFor(usage), usage, dummySize);
        dummy->setName(aux.is_uniform ? "RRP_ubo_dummy" : "RRP_aux_dummy");
        if(!dummy->create())
          qWarning() << "RawRaster: could not create the placeholder buffer for"
                     << aux.name.c_str();
        else if(!auxPlaceholderZeroFillDisabled())
          // Zero-fill. Vulkan does NOT initialise VkBuffer memory: a placeholder
          // allocated on a RenderList rebuild lands on whatever the previous
          // owner of that suballocation left behind (measured on an RTX 4090:
          // a freshly created, never-uploaded 256-byte Dynamic UBO reads back
          // the byte pattern of a UBO freed earlier in the same process).
          // When the aux has no producer in the user's graph this placeholder
          // IS the buffer the shader reads, and shaders read it as a SENTINEL:
          // classic_pbr_openpbr gates its clustered-lighting and volumetric
          // paths on `cluster_config.cluster_x == 0u`, then indexes
          // cluster_light_counts / cluster_light_lists / vol_integrated with an
          // id derived from that grid. Garbage there turns a 16-byte
          // placeholder into a multi-gigabyte out-of-bounds read. Same
          // Vulkan-doesn't-zero-VkBuffers reasoning, and the same helper, as
          // the INPUTS-side placeholders in
          // IsfBindingsBuilder::ensureStorageResources -- that fix only ever
          // covered the INPUTS storage/uniform path, never top-level AUXILIARY.
          RhiClearBuffer::clearBuffer(rhi, res, dummy, 0, (quint32)dummySize);
        traceAuxPlaceholder(
            aux.name, dummySize, aux.is_uniform, !auxPlaceholderZeroFillDisabled());
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
    if(m_mesh && m_mesh->hasGeometry())
      m_mesh->preparePipeline(*ps);

    // Compute effective pipeline state: the descriptor's PIPELINE_STATE (if
    // any) wins over the legacy material-UBO-driven blend. When no state is
    // declared (empty pipeline_state) we keep the legacy behaviour: blending
    // driven by the material's runtime-editable blend UI + hardcoded depth
    // test/write.
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

    // The material 'mode' control seeds the topology, but an EXPLICITLY
    // declared PIPELINE_STATE TOPOLOGY wins -- same precedence rule as
    // blend ("applyPipelineState only overrides blend when BLEND was
    // explicitly declared"). Before this, the unconditional switch below ran
    // AFTER applyPipelineState and silently clobbered every declared
    // TOPOLOGY (measured: a RAW_RASTER shader with
    // PIPELINE_STATE {TOPOLOGY: points} still drew triangles --
    // tests/gfx/GfxPointCloudCount.cpp).
    if(!desc.default_state.topology.has_value())
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

    // Remap vertex inputs by semantic, honouring explicit SEMANTIC overrides
    // declared on VERTEX_INPUTS. Skipped for procedural draws.
    //
    // The fallback-aware overload resolves "REQUIRED: false" inputs missing
    // from upstream geometry to a shared PerInstance identity buffer from the
    // RenderList's pool; with no opt-ins the plan is empty and costs nothing.
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

    // A mesh whose geometry was filtered away has an empty vertex-input layout,
    // which cannot satisfy a vertex shader that declares inputs
    // (VUID-VkGraphicsPipelineCreateInfo-Input-07904), and there is nothing to
    // draw. Drop the pass; it is rebuilt when geometry comes back.
    const bool meshEmpty = m_mesh && !m_mesh->hasGeometry();
    if(meshEmpty || !ps->create())
    {
      if(!meshEmpty)
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
      // The Pass owns both when it is stored; when it is not, both leak.
      delete bindings;
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

  // Tear down state left by a previous init pass. update() releases
  // m_mrtRenderTarget but not our private per-mip / per-face RT pool or the
  // CUBEMAP+MULTIVIEW shim's cube handle, and an m_mipRTs entry pointing at a
  // freed shadow array crashes NVIDIA's driver inside CmdBeginRenderPass.
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

  // PerLayer resources. Both paths now keep their per-layer render targets in
  // m_mipRTs (cleared above); the depth path's entries alias the OUTPUT depth
  // array through setDepthLayer and own no depth texture of their own, so
  // entry.depth is null for them. Only the shared placeholder colour is ours.
  if(m_perLayerDummyColor)
  {
    m_perLayerDummyColor->deleteLater();
    m_perLayerDummyColor = nullptr;
  }
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

  // Honour OUTPUTS.WIDTH / HEIGHT when declared, otherwise the renderer's
  // render size. A RAW_RASTER_PIPELINE shader has one shared render pass, so
  // every attachment ends up at the size of the first explicitly sized OUTPUT.
  // Unsized outputs inherit it; differing explicit sizes are a shader-author
  // error and are not diagnosed here.
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
      // PER_MIP and PER_CUBE_FACE apply to colour outputs only: depth
      // attachments have no mip chain here and cube depth would need its own
      // path. PER_LAYER takes either, so it walks the raw outputs[] to include
      // depth entries while the other two keep the colour-only walk.
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
    //
    // ... but only while multiview actually amplifies anything. Where the view
    // index has been lowered to PASSINDEX there is no amplification left, and
    // the explicit per-face loop is the ONLY thing that writes the other five
    // faces. Disabling it there is what left five of six faces unwritten.
    const bool mvLowered = viewIndexNeedsPassIndexFallback(
        renderer.state.api, renderer.state.version);
    if(m_executionMode == ExecutionMode::PerCubeFace
       && n.descriptor().multiview_count >= 2 && !mvLowered)
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

    // MULTIVIEW:N with no EXECUTION_MODEL at all is the common shape -- the
    // shader just declares MULTIVIEW and lets one draw fan out over the layers
    // (syn-cube-six-colors, syn-camera-array-faces). When the view index is
    // lowered, that fan-out is gone and nothing replaces it: the node writes
    // layer 0 and leaves the rest at their clear colour, which reads back as
    // "four of six faces missing" rather than as a disabled feature.
    //
    // Promote such a node to the explicit loop that the lowering assumes:
    // PER_CUBE_FACE for a cube output, PER_LAYER for a plain layered one.
    // Both already exist, already build one render target per layer, and
    // already stamp the invocation index into ProcessUBO::passIndex -- which
    // is exactly what the lowered shader now reads.
    const int mvDecl = n.descriptor().multiview_count;
    if(mvLowered && mvDecl >= 2 && m_executionMode == ExecutionMode::Single)
    {
      int colorIdx = 0;
      for(int i = 0; i < (int)outputs.size(); ++i)
      {
        const auto& out = outputs[i];
        if(out.type == "depth")
          continue;
        if(out.is_cubemap)
        {
          m_executionMode = ExecutionMode::PerCubeFace;
          m_perCubeFaceOutputIndex = colorIdx;
          break;
        }
        if(out.layers >= mvDecl)
        {
          m_executionMode = ExecutionMode::PerLayer;
          m_perLayerOutputIndex = i;
          m_perLayerIsDepth = false;
          break;
        }
        ++colorIdx;
      }
    }
  }

  // Layered / multiview detection, same shape as SimpleRenderedISFNode:
  // LAYERS: N on any OUTPUT gives an N-layer texture array; MULTIVIEW: N on the
  // descriptor makes one draw write N views, and requires caps.multiview.
  int maxLayers = 1;
  for(const auto& out : outputs)
    if(out.layers > maxLayers)
      maxLayers = out.layers;
  const int mvCount = n.descriptor().multiview_count;
  const bool wantMultiview
      = mvCount >= 2 && renderer.state.caps.multiview
        && !viewIndexNeedsPassIndexFallback(
            renderer.state.api, renderer.state.version);
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

      // CUBEMAP + MULTIVIEW: QRhi forbids multiview on a cube texture, so render
      // into a UsedAsTransferSource 2D TextureArray, which multiview accepts,
      // and stamp a CubeMap alongside for downstream sampling; each array layer
      // is copied into the matching face once the pass ends. Only one output
      // gets the cube-copy treatment.
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
        // Cube faces must be square: CUBE_COMPATIBLE images require
        // extent.width == extent.height. Force the face to min(w, h) for
        // non-square render targets and size the shadow array to match, so the
        // multiview draw writes the full face.
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

        // The downstream-visible cube. Same format, no RenderTarget flag -- it
        // is only ever copied into. MipMapped is forwarded so a prefilter chain
        // can be generated downstream, and UsedWithGenerateMips lets the
        // end-of-frame generateMips() reach it; the shadow array is never
        // sampled downstream and needs neither.
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
    // Depth-only shader (e.g. shadow_cascades.frag). Build the RT AROUND the
    // node-owned depth texture (possibly a TextureArray) instead of letting
    // the helper allocate one and then deleting it while the render pass
    // still references it (use-after-free + never-rendered output texture).
    m_mrtRenderTarget = createDepthOnlyRenderTarget(
        renderer.state, depthTex, mrtSamples, /*samplableDepth=*/true);
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
    // Attach ALL color textures so attachments == pipeline blend targets.
    m_mrtRenderTarget = createMultiViewRenderTarget(
        renderer.state,
        std::span<QRhiTexture* const>{colorTextures.data(), colorTextures.size()},
        mvCount, depthTex, mrtSamples);
  }
  else if(maxLayers > 1 && !colorTextures.empty())
  {
    // Layered but not multiview — render to layer 0 by default; downstream
    // per-pass LAYER selection (once PASSES loop lands) will pick others.
    // Attach ALL color textures so attachments == pipeline blend targets.
    m_mrtRenderTarget = createLayeredRenderTarget(
        renderer.state,
        std::span<QRhiTexture* const>{colorTextures.data(), colorTextures.size()},
        0, depthTex, mrtSamples);
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
      // Sample count must match the single-sample color attachments above,
      // or renderTarget->create() fails.
      m_mrtRenderTarget.depthTexture = rhi.newTexture(
          QRhiTexture::D32F, sz, 1,
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

  // PER_CUBE_FACE: one render target per cube face, each attaching the same
  // cube texture via setLayer(i). Structurally the PER_MIP path with a CubeMap
  // target; m_mipRTs is reused as storage, its index meaning face here and mip
  // level there. Mutually exclusive with PER_MIP.
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

  // PER_MIP: one render target per mip level of the target output, attached via
  // setLevel(i); runInitialPasses iterates them in order and injects the mip
  // index as ProcessUBO.passIndex. Multiview propagates: with MULTIVIEW:6 each
  // mip's attachment also carries setMultiViewCount(6). Depth is a per-mip
  // single-sample D32F so the render-pass contract is the same at every level.
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

        // Depth must match the multiview shape: a plain 2D depth attachment
        // against a multiview colour attachment fails QRhi's render-pass compat
        // check. Each mip gets its own depth so the attachment size matches the
        // colour attachment's mip-i size.
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

  // PER_LAYER: one render target per layer of the target output's TextureArray;
  // runInitialPasses iterates them in order and injects the layer index as
  // ProcessUBO.passIndex. Drives shadow_cascades.
  //
  // A COLOR target works like PER_CUBE_FACE with a variable layer count:
  // m_mipRTs holds N entries bound via setLayer(i), with a per-layer 2D D32F
  // depth so attachment shapes stay consistent.
  //
  // A DEPTH target works the same way from Qt 6.12, which added
  // QRhiTextureRenderTargetDescription::setDepthLayer: each layer gets its own
  // render target attaching layer i of the OUTPUT depth array directly.
  //
  // It used to render to a shared scratch 2D D32F and copyTexture() it into
  // layer i after each endPass. That shim NEVER WORKED on any backend and is
  // not preserved: QRhi::copyTexture is colour-only -- qrhivulkan.cpp:4782 and
  // :4792 set VK_IMAGE_ASPECT_COLOR_BIT unconditionally (VUID-vkCmdCopyImage-
  // aspectMask-00142/00143), and the GL path attaches the source to
  // GL_COLOR_ATTACHMENT0 -- so the depth array came back cleared and the
  // cascade rendered nothing.
  if(m_executionMode == ExecutionMode::PerLayer && m_perLayerOutputIndex >= 0)
  {
    const auto& targetOut = outputs[m_perLayerOutputIndex];
    const int layerCount = std::max(1, targetOut.layers);

    if(m_perLayerIsDepth)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
      // depthTex is the OUTPUT array (allocated as Texture2DArray earlier when
      // maxLayers > 1). Each layer gets a render target that attaches it
      // directly, so the pass writes the real destination and nothing is
      // copied anywhere.
      if(depthTex && layerCount > 1)
      {
        // Mirror createDepthOnlyRenderTarget's attachment shape, since the
        // pipeline is built against the render pass that helper produced and
        // must stay compatible with these. It attaches a dummy RGBA8 colour
        // alongside the depth, required by GLES and harmless elsewhere.
        //
        // SIZED TO THE RENDER EXTENT, NOT 1x1. The old scratch RT allocated it
        // at QSize(1, 1); the Vulkan backend derives the framebuffer and
        // renderArea from the FIRST colour attachment whenever colorAttCount >
        // 0 (qrhivulkan.cpp:8619-8620, :8782-8783) and only falls back to the
        // depth texture's size at colorAttCount == 0, so that would have
        // clamped every cascade to one pixel. It was invisible because the
        // copy that followed was a no-op anyway; the same lesson is already
        // written into createDepthOnlyRenderTarget (Utils.cpp:1586-1590). One
        // texture is shared by all N targets: it is never written or read.
        m_perLayerDummyColor = rhi.newTexture(
            QRhiTexture::RGBA8, sz, 1, QRhiTexture::RenderTarget);
        m_perLayerDummyColor->setName(
            ("RRPNode::MRT::perLayerDummyColor::" + targetOut.name).c_str());
        SCORE_ASSERT(m_perLayerDummyColor->create());

        m_mipRTs.reserve(layerCount);
        for(int layer = 0; layer < layerCount; ++layer)
        {
          QRhiTextureRenderTargetDescription layerDesc;
          {
            QRhiColorAttachment color0(m_perLayerDummyColor);
            layerDesc.setColorAttachments({color0});
          }
          layerDesc.setDepthTexture(depthTex);
          layerDesc.setDepthLayer(layer);

          auto* layerRT = rhi.newTextureRenderTarget(layerDesc);
          layerRT->setName(
              ("RRPNode::MRT::perLayerDepthRT::" + std::to_string(layer))
                  .c_str());
          auto* layerRP = layerRT->newCompatibleRenderPassDescriptor();
          layerRP->setName(
              ("RRPNode::MRT::perLayerDepthRP::" + std::to_string(layer))
                  .c_str());
          layerRT->setRenderPassDescriptor(layerRP);
          SCORE_ASSERT(layerRT->create());

          MipRT entry;
          entry.renderTarget = layerRT;
          entry.renderPass = layerRP;
          // The depth is the OUTPUT array, owned by m_mrtRenderTarget: this
          // entry only points at one of its layers and must not free it.
          entry.depth = nullptr;
          m_mipRTs.push_back(entry);
        }

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
#else
      // Below Qt 6.12 there is no per-layer depth attachment, and there is no
      // working substitute: the copyTexture shim that used to stand here was a
      // no-op on every backend (copyTexture is colour-only), so the cascade
      // array came back cleared and the shadows silently disappeared. Refuse
      // the mode out loud instead. Releases target Qt 6.12+.
      qWarning()
          << "RawRaster EXECUTION_MODEL=PER_LAYER: depth target"
          << QString::fromStdString(targetOut.name)
          << "requires Qt 6.12 or newer (QRhiTextureRenderTargetDescription::"
             "setDepthLayer); this build is"
          << QT_VERSION_STR
          << "- the per-layer depth cascade is DISABLED for this node. "
             "Cascaded shadows will not render.";
      m_executionMode = ExecutionMode::Single;
#endif
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

  QRhiBuffer* pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("RenderedRawRasterPipelineNode::initMRTPass::pubo");
  pubo->create();

  try
  {
    auto [v, s] = score::gfx::makeShaders(
        renderer.state, n.m_vertexS, n.m_fragmentS, n.descriptor().multiview_count);

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
      // otherwise). Mirrors the non-MRT path, including the LAYOUT-derived
      // size.
      if(!aux.buffer)
      {
        auto usage = aux.is_uniform ? QRhiBuffer::UniformBuffer
                                    : QRhiBuffer::StorageBuffer;
        // Rounded up to 4: RhiClearBuffer's contract (vkCmdFillBuffer) wants a
        // 4-byte-aligned size.
        const int64_t dummySize
            = (std::max<int64_t>(aux.declared_size, aux.is_uniform ? 256 : 16) + 3)
              & ~int64_t(3);
        auto* dummy = rhi.newBuffer(bufferTypeFor(usage), usage, dummySize);
        dummy->setName(aux.is_uniform ? "RRP_ubo_dummy" : "RRP_aux_dummy");
        if(!dummy->create())
          qWarning() << "RawRaster: could not create the placeholder buffer for"
                     << aux.name.c_str();
        else if(!auxPlaceholderZeroFillDisabled())
          // Zero-fill: an unwritten placeholder reads back recycled device
          // memory, and the shader reads it as a sentinel. Same reasoning as
          // the non-MRT path.
          RhiClearBuffer::clearBuffer(rhi, res, dummy, 0, (quint32)dummySize);
        traceAuxPlaceholder(
            aux.name, dummySize, aux.is_uniform, !auxPlaceholderZeroFillDisabled());
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

    // PerMip / PerCubeFace / PerLayer-colour draw exclusively into the
    // per-iteration RTs in m_mipRTs, so the pipeline must be built against
    // their render pass (1 colour attachment, 1 sample) to satisfy QRhi's
    // compatibility check. The PerLayer depth path leaves m_mipRTs empty and
    // mirrors m_mrtRenderTarget's attachment shape.
    QRhiRenderPassDescriptor* pipelineRP = m_mrtRenderTarget.renderPass;
    int pipelineColorCount = m_mrtRenderTarget.colorAttachmentCount();
    int pipelineSamples = m_mrtRenderTarget.sampleCount() > 0
                              ? m_mrtRenderTarget.sampleCount()
                              : renderer.samples();
    if(m_executionMode != ExecutionMode::Single && !m_mipRTs.empty()
       && m_mipRTs[0].renderPass)
    {
      pipelineRP = m_mipRTs[0].renderPass;
      pipelineColorCount = 1;
      pipelineSamples = 1;
    }
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
    if(m_mesh && m_mesh->hasGeometry())
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
      for(int i = 0; i < std::max(1, pipelineColorCount); i++)
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
          *ps, desc.default_state, pipelineColorCount,
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
      for(int i = 0; i < std::max(1, pipelineColorCount); i++)
        blends.append(premulAlphaBlend);
      ps->setTargetBlends(blends.begin(), blends.end());

      ps->setDepthTest(true);
      ps->setDepthWrite(true);
      // Reverse-Z project rule.
      ps->setDepthOp(QRhiGraphicsPipeline::Greater);
    }

    // Same precedence rule as the single-target pass above: an explicitly
    // declared PIPELINE_STATE TOPOLOGY wins over the material mode control.
    if(!desc.default_state.topology.has_value())
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

    SCORE_ASSERT(pipelineRP);
    ps->setRenderPassDescriptor(pipelineRP);

    // A mesh whose geometry was filtered away has an empty vertex-input layout,
    // which cannot satisfy a vertex shader that declares inputs
    // (VUID-VkGraphicsPipelineCreateInfo-Input-07904), and there is nothing to
    // draw. Drop the pass; it is rebuilt when geometry comes back.
    const bool meshEmpty = m_mesh && !m_mesh->hasGeometry();
    if(meshEmpty || !ps->create())
    {
      if(!meshEmpty)
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
      // The Pass owns both when it is stored; when it is not, both leak.
      delete bindings;
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

  const bool srcIsArray = srcTex && (srcTex->flags() & QRhiTexture::TextureArray);
  auto [vertexS, fragmentS] = score::gfx::makeShaders(
      renderer.state, rrp_blit_vs, srcIsArray ? rrp_blit_array_fs : rrp_blit_fs);

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

  SCORE_ASSERT(m_passes.empty());
  SCORE_ASSERT(m_inputSamplers.empty());
  SCORE_ASSERT(m_audioSamplers.empty());

  m_inputSamplers = initInputSamplers(this->n, renderer, n.input, &n.descriptor());

  // Build the auxiliary-texture binding table and seed the initial texture
  // pointers from the incoming geometry, recording a (sampler_idx, name) pair
  // for every image-style INPUT a geometry aux texture might serve. update()
  // re-runs the lookup on geometry change, so rebuilt or grown channel arrays
  // flow through without a cable.
  bindAuxTexturesInit(renderer);

  m_audioSamplers = initAudioTextures(renderer, n.m_audio_textures);

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
        const auto type = bufferTypeFor(usage);
        auto* buf = rhi.newBuffer(type, usage, sz);
        buf->setName(QByteArray("RRP_aux_") + ssbo.name.c_str());
        if(!buf->create())
        {
          qWarning() << "RawRaster: could not create the auxiliary buffer for"
                     << ssbo.name.c_str();
          delete buf;
          return;
        }
        // uploadStaticBuffer is only defined for non-Dynamic buffers, and a
        // uniform block is Dynamic everywhere -- see bufferTypeFor.
        if(type == QRhiBuffer::Dynamic)
          res.updateDynamicBuffer(buf, 0, (quint32)sz, cpu->raw_data.get());
        else
          res.uploadStaticBuffer(buf, 0, sz, cpu->raw_data.get());
        ssbo.buffer = buf;
        ssbo.size = sz;
        ssbo.owned = true;
      }
    };

    // Resolve a buffer for `ssbo` by scanning the connected input port's edges
    // for an upstream producer. Upstream renderers publish through the virtual
    // NodeRenderer::bufferForOutput() -- Port::value is never written for
    // buffer-typed outputs -- so retrieval goes through
    // RenderList::bufferForInput(edge).
    //
    // Complements try_bind_from_geometry: an INPUTS-declared storage_input or
    // uniform_input may be wired through a dedicated Buffer edge instead of
    // riding along with the geometry.
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

    // INPUTS storage_input / uniform_input have a matching score input port
    // from ISFNode's isf_input_port_vis; record its index so update() can
    // re-pull the upstream buffer when it changes.
    //
    // walk_descriptor_inputs() advances the cumulative port_counts in lockstep
    // with isf_input_port_vis (see ISFVisitors.hpp). For RawRaster the cursor
    // starts at 1: port 0 is the mandatory Geometry input.
    //
    // GLSL emits desc.inputs before top-level AUXILIARY, so AuxiliarySSBOs are
    // pushed in that order; reversing shifts every binding index by
    // desc.auxiliary.size() and Vulkan rejects the pipeline.
    const bool isRawRaster = (desc.mode == isf::descriptor::RawRaster);
    const port_counts startPC{isRawRaster ? 1 : 0, 0, 0};
    // INPUTS storage_input / csf_image_input / uniform_input go through
    // IsfBindingsBuilder's m_storage path (allocateStorageResources +
    // buildExtraBindings) so the SRB binding type matches what
    // isf_emit_graphics_storage emits in GLSL.
    //
    // The walker below is still needed for indirect_draw storage_input, which
    // is special-cased at runtime and has no SRB binding.
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
    // Read-only csf_image_input adopts the matching upstream auxiliary_texture
    // by name -- the storage image an upstream CSF or RawRaster published into
    // its out_geo -- and the helper frees the auto-allocated placeholder. The
    // SRB does not exist at init time; it is patched in update(). INPUTS
    // storage_input / uniform_input name-match against the geometry's
    // auxiliary_buffers the same way, which is how ScenePreprocessor publishes
    // scene_lights, per_draws, scene_counts, camera and env into
    // flattened-scene shaders.
    if(geometry.meshes && !geometry.meshes->meshes.empty())
    {
      bindUpstreamImagesFromGeometry(m_storage, geometry.meshes->meshes[0]);
      bindUpstreamBuffersFromGeometry(
          *renderer.state.rhi, res, m_storage, geometry.meshes->meshes[0]);
    }

    // Top-level AUXILIARY entries have no score input port; they are resolved
    // by name from the upstream geometry's auxiliary list. is_uniform picks
    // std140 UBO over std430 SSBO, and downstream allocation and SRB-build
    // sites dispatch on the flag AuxiliarySSBO already carries. Persistent
    // entries own a ping-pong pair, SSBO only -- the parser treats
    // UBO + persistent as a no-op, so this branch is gated on !is_uniform.
    //
    // GLSL emits these after all INPUTS bindings, so they are pushed after the
    // INPUTS loop to keep binding slots aligned.
    //
    // aux_declared_size is the byte size the shader's own LAYOUT implies, so an
    // unresolved aux still gets a placeholder covering every member it reads.
    auto aux_declared_size
        = [&](const isf::geometry_input::auxiliary_request& aux) -> int64_t {
      int64_t count = 0;
      if(!aux.is_uniform && !aux.size.empty())
      {
        try
        {
          count = std::max<int64_t>(1, std::stoll(aux.size));
        }
        catch(const std::exception&)
        {
          count = 1024; // TODO: evaluate $USER when we add it
        }
      }
      return aux.is_uniform
                 ? score::gfx::calculateUniformBlockSize(aux.layout, (int)count, desc)
                 : score::gfx::calculateStorageBufferSize(aux.layout, (int)count, desc);
    };

    for(const auto& aux : desc.auxiliary)
    {
      AuxiliarySSBO ssbo;
      ssbo.name = aux.name;
      ssbo.access = aux.access;
      ssbo.persistent = aux.persistent && !aux.is_uniform;
      ssbo.is_uniform = aux.is_uniform;
      ssbo.declared_size = aux_declared_size(aux);

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

      // Resolution census, printed before any placeholder exists: which
      // AUXILIARY names the upstream geometry actually published and which the
      // node will have to invent a buffer for. Whoever reads the
      // [AUX-PLACEHOLDER] lines below needs this list to know the trace was in
      // a position to observe anything at all.
      traceAuxResolution(ssbo.name, ssbo.buffer != nullptr, ssbo.size);

      m_auxiliarySSBOs.push_back(std::move(ssbo));
    }
  }

  // MRT is needed for anything the single-target path cannot express: several
  // colour attachments, an explicit depth output, layered or cubemap output, or
  // multiview -- the last because its render target has a different shape from
  // a swapchain RT.
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
    if(m_mrtRenderTarget.texture == nullptr)
    {
      initMRTPass(renderer, res);
    }

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

  // PerLayer state — same shape as the init-time cleanup in update(). Both
  // paths keep their per-layer render targets in m_mipRTs (cleared above); the
  // depth path's entries alias layers of the OUTPUT depth array and own no
  // depth of their own. Only the shared placeholder colour is ours.
  if(m_perLayerDummyColor)
  {
    m_perLayerDummyColor->deleteLater();
    m_perLayerDummyColor = nullptr;
  }
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
  // Evict the cached per-(port, source) geometry first (base class): without
  // it the departed producer's spec lingers in m_portGeometries. Same P0-9
  // class as RenderedCSFNode::removeInputEdge.
  NodeRenderer::removeInputEdge(renderer, edge);
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
  else if(edge.sink->type == Types::Geometry && edge.sink->edges.size() <= 1)
  {
    // The LAST geometry feed of this port is going away (called before edge
    // destruction, so the departing edge is still in the list). For a
    // GPU-produced mesh the vertex/index buffers the acquired CustomMesh
    // binds are owned by the departing producer's renderer and die with it
    // -- keeping the mesh meant vkCmdBindVertexBuffers on freed buffers
    // (P0-9, tests/gfx/GfxGeometryProducerRemoval.cpp). Drop the cached
    // spec and the acquired mesh; the draw path already handles a null
    // m_mesh ("m_mesh stays null and the draw call doesn't run") and the
    // pass is rebuilt when geometry comes back.
    this->geometry = {};
    m_mesh = nullptr;
    m_meshbufs = {};
    this->geometryChanged = true;
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
  // Update node materials: must run before any early return.
  bool mustRecreatePasses = updateMaterials(renderer, res, edge);
  bool recreateDueToMaterial = mustRecreatePasses;

  // Refresh upstream-bound storage_input / uniform_input buffers from input
  // ports. The first pass will pick them up via the SRB; subsequent passes
  // need bindUpstreamBuffers to patch their SRBs in-place — handled per-pass
  // when m_passes is iterated for SRB updates further down. (Safe to call
  // even with no SRB; the helper just refreshes the m_storage entries.)
  bindUpstreamBuffers(renderer, n.input, m_storage);
  // Same for read-only csf_image_input: adopt the matching upstream
  // auxiliary_texture. Called per frame so a producer that swaps its
  // QRhiTexture on resize or rebuild flows through. The helper is idempotent
  // and patches every SRB it is given, so one call per pass refreshes them all
  // while the upstream lookup happens on the first iteration only.
  if(geometry.meshes && !geometry.meshes->meshes.empty())
  {
    // Per-pass refresh of the name-matched-from-geometry bindings (SSBO, UBO,
    // storage image). bindUpstream*FromGeometry are idempotent and patch each
    // SRB unconditionally.
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
    // Mirror onto the per-invocation SRB pool (PER_LAYER / PER_MIP /
    // MANUAL COUNT>1 clone the main SRB): invocations 1..N-1 own separate
    // SRBs and must pick up the same geometry-published buffer/image swaps,
    // otherwise they keep the stale (possibly deleteLater'd) upstream handle
    // -> UAF/garbage on all layers/mips but the first when upstream reallocs.
    for(auto* invSrb : m_perInvocationSRBs)
    {
      if(!invSrb)
        continue;
      bindUpstreamImagesFromGeometry(
          m_storage, geometry.meshes->meshes[0], invSrb);
      bindUpstreamBuffersFromGeometry(
          *renderer.state.rhi, res, m_storage,
          geometry.meshes->meshes[0], invSrb);
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

    // Passes were just rebuilt, so their SRBs reference the current m_storage
    // entries -- which, for name-matched-from-geometry buffers, may still hold
    // the 16-byte zero placeholder ensureStorageResources allocated: the
    // per-pass refresh loop below is gated on m_passes being non-empty, and on
    // a fresh RenderList the first frame ran initState with m_passes empty.
    // Re-fire bindUpstream*FromGeometry on the new SRBs so they pick up the
    // live geometry buffers and textures on this frame rather than the next.
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
      // Mirror onto the per-invocation SRB pool (see the symmetric loop in
      // the per-frame refresh above): invocations 1..N-1 own separate SRBs
      // and must pick up the same freshly-bound geometry buffers/images.
      for(auto* invSrb : m_perInvocationSRBs)
      {
        if(!invSrb)
          continue;
        bindUpstreamImagesFromGeometry(
            m_storage, geometry.meshes->meshes[0], invSrb);
        bindUpstreamBuffersFromGeometry(
            *renderer.state.rhi, res, m_storage,
            geometry.meshes->meshes[0], invSrb);
      }

      // Re-run rebindAuxTextures here. It is idempotent, short-circuiting when
      // the slot's cached texture matches the upstream's current one. On true,
      // hot-patch the existing SRBs with replaceTexture instead of another
      // mustRecreatePasses cycle: the pipeline layout is unchanged.
      if(rebindAuxTextures())
      {
        // The replaceTexture match key must be the sampler actually in the SRB
        // binding. allSamplers() substitutes m_inputSamplerOverrides[i] when
        // ScenePreprocessor publishes a per-bucket sampler_handle, so each
        // material's wrap/filter survives; replaceTexture matches by sampler
        // pointer (Utils.cpp), and passing the original sampler as the key when
        // the SRB holds the override silently no-ops.
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
  std::copy_n(renderer.currentDate, 4, n.standardUBO.date);

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

  // initInputSamplers walks n.input[] and pushes one sampler per Types::Image
  // port, plus a depth sampler when the port has SamplableDepth.
  // walk_descriptor_inputs supplies the canonical sampler delta per input, so
  // each image-like INPUT lands on its matching slot.
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

  // Path A: texture overrides on input-port-backed samplers -- an INPUTS image
  // whose name matches a geometry aux texture gets its sampler's texture
  // swapped. When the geometry also publishes a sampler_handle, swap that too:
  // that is how ScenePreprocessor's per-bucket samplers take effect.
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
    // The override is non-owning: the bucket in GpuResourceRegistry owns the
    // QRhiSampler. Kept in the parallel m_inputSamplerOverrides so
    // m_inputSamplers still holds the sampler release() deletes.
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
    // One destroy+setBindings+create per pass however many aux handles changed:
    // the per-texture replaceTexture overload rebuilds the SRB each call, so N
    // of them means N full rebuilds per pass per frame.
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
    // Per-invocation SRB pool (PerMip / PerCubeFace / Manual): clones of
    // pass.p.srb taken at construction. Invocation 0 renders through
    // pass.p.srb, so without this mirror invocations 1..N-1 keep sampling the
    // stale handle.
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
  // MDI readback fallback for backends without drawIndirect: synchronously read
  // back the GPU indirect buffer so the CPU draw loop has this frame's
  // commands.
  //
  // Re-runs every frame because the indirect buffer is GPU-generated, e.g. by a
  // culling compute pass; gating on cpuDrawCommands.empty() would freeze the
  // draw list after the first readback.
  //
  // Guarded on ReadBackNonUniformBuffer, which OpenGL ES 2.0 lacks. Without it
  // the draw falls back to whatever cpuDrawCommands holds, or a single
  // drawIndexed, and warns once.
  if(m_meshbufs.useIndirectDraw
     && !m_meshbufs.gpuIndirectSupported
     && m_meshbufs.indirectDrawBuffer
     && m_meshbufs.indirectDrawBuffer->size() > 0
     && renderer.state.rhi->isFeatureSupported(QRhi::ReadBackNonUniformBuffer))
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
  else if(
      m_meshbufs.useIndirectDraw && !m_meshbufs.gpuIndirectSupported
      && m_meshbufs.indirectDrawBuffer && m_meshbufs.indirectDrawBuffer->size() > 0
      && !renderer.state.rhi->isFeatureSupported(QRhi::ReadBackNonUniformBuffer))
  {
    // Graceful degradation: the backend (e.g. OpenGL ES 2.0) can neither
    // draw indirect nor read back the GPU-generated indirect buffer. The draw
    // loop falls back to cpuDrawCommands (if a producer ever filled them) or a
    // single drawIndexed. Warn once so the missing GPU-culled commands are
    // diagnosable rather than a silent visual divergence.
    static bool warned = false;
    if(!warned)
    {
      warned = true;
      qWarning() << "RenderedRawRasterPipelineNode: GPU-generated indirect draws "
                    "require QRhi::ReadBackNonUniformBuffer, unsupported on this "
                    "backend (e.g. OpenGL ES 2.0) — falling back to CPU draw "
                    "commands; GPU culling output will not be reflected.";
    }
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

  // MRT: render into the internal multi-attachment target. The MRT pass is the
  // one initMRTPass registered with a null edge; the blit passes that follow
  // each carry their own. Index 0 is not a stand-in -- initMRTPass only
  // registers its pass when the pipeline was created, so a driver-rejected
  // shader leaves a blit pass there, and pairing its non-layered target with
  // the MRT pipeline state segfaults inside QRhi::beginPass.
  auto mrt_it = ossia::find_if(m_passes, [](const auto& p) { return p.first == nullptr; });
  if(mrt_it == m_passes.end())
    return;

  auto& pass = mrt_it->second;

  SCORE_ASSERT(pass.renderTarget.renderTarget);
  SCORE_ASSERT(pass.p.pipeline);
  SCORE_ASSERT(pass.p.srb);

  // Invocation count: Single is 1, PerMip / PerCubeFace use m_mipCount (mip
  // count or 6 faces), Manual evaluates the COUNT expression, falling back to 1
  // when it is empty or unparseable. Manual re-evaluates every frame so the
  // count tracks live input values.
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
  // Depth-only shaders have no colour attachment, so mainTex is null: fall back
  // to the depth attachment for the render-target size, then to the renderer's
  // render size. PER_LAYER+depth declares WIDTH/HEIGHT on its depth output
  // (2048x2048 for shadow maps) and the viewport must honour that.
  QRhiTexture* sizeTex = mainTex
                             ? mainTex
                             : pass.renderTarget.depthTexture;
  const QSize baseSize
      = sizeTex ? sizeTex->pixelSize() : renderer.state.renderSize;

  QRhi& rhi = *renderer.state.rhi;

  // Grow the per-invocation UBO+SRB pool when invocationCount exceeds what is
  // allocated. Each extra UBO gets its own dynamic slot -- QRhi Dynamic UBOs
  // have a single slot, so aliasing one buffer collapses PASSINDEX to the
  // last-written value. SRB i clones the main SRB with the process-UBO binding
  // swapped to UBO i.
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

    // Clone the main SRB's bindings and swap binding=1, the process UBO per ISF
    // convention, to the new buffer. The main pass's SRB defines the layout;
    // the clones are structurally identical and stay pipeline-compatible.
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
      // Both paths are now one RT per layer in m_mipRTs, same shape as
      // PerCubeFace: the colour path binds layer i with setLayer(), the depth
      // path binds layer i of the OUTPUT depth array with setDepthLayer(). The
      // pass writes its destination directly, so nothing is copied afterwards.
      if(i < (int)m_mipRTs.size() && m_mipRTs[i].renderTarget)
      {
        rtForPass = m_mipRTs[i].renderTarget;
      }
    }

    // rtForPass starts as the node's own render target and is replaced by a
    // per-mip / per-face / per-layer one above, each branch guarded on the RT
    // existing. When none matches it can still be null, and Qt segfaults inside
    // beginPass rather than rejecting it, so skip the pass.
    if(!rtForPass)
    {
      qWarning() << "RenderedRawRasterPipelineNode: pass" << i
                 << "has no render target; skipping it";
      continue;
    }

    const auto declaredCompare
        = n.descriptor().default_state.depth_compare
              ? toCompareOp(*n.descriptor().default_state.depth_compare)
              : QRhiGraphicsPipeline::Greater;
    cb.beginPass(
        rtForPass, Qt::transparent,
        {depthClearForCompare(declaredCompare), 0}, invBatch);

    cb.setGraphicsPipeline(pass.p.pipeline);
    cb.setViewport(
        QRhiViewport(0, 0, viewportSize.width(), viewportSize.height()));

    // drawWithPerMeshAuxRebind sets shader resources and issues the
    // draw call (or the per-sub-mesh loop for multi-mesh inputs).
    // Pass the per-invocation SRB so each draw reads its own UBO.
    // Forward the pass's fallback-binding plan so "REQUIRED: false"
    // VERTEX_INPUTS get their identity buffers bound.
    drawWithPerMeshAuxRebind(*invSRB, cb, pass.fallback_bindings);

    cb.endPass();
  }

  // CUBEMAP + MULTIVIEW finaliser: once every pass has ended, copy each layer of
  // the shadow TextureArray into the matching face of the public CubeMap. QRhi
  // face order is +X, -X, +Y, -Y, +Z, -Z, matching gl_ViewIndex, so layer i
  // maps to face i. With PER_MIP also active both are MipMapped and the loop
  // covers the whole chain: N * 6 pure GPU blits.
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

  // GENERATE_MIPS: call generateMips() on every declared OUTPUT target. For
  // cube-copy outputs that is the public cube, not the shadow array, which
  // downstream never samples and may not even be MipMapped. Skipped when
  // PER_MIP is active on the same output: the render loop already authored
  // distinct content per mip.
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
  // Debug marker for capture-tool readability (RenderDoc /
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

      drawWithPerMeshAuxRebind(*srb, cb, pass.fallback_bindings);
    }
  }
}

void RenderedRawRasterPipelineNode::process(int32_t port, const ossia::transform3d& v)
{
  m_modelTransform = v;
}

void RenderedRawRasterPipelineNode::drawWithPerMeshAuxRebind(
    QRhiShaderResourceBindings& srb, QRhiCommandBuffer& cb,
    const FallbackBindingPlan& plan)
{
  // ScenePreprocessor's output geometry is always a single sub-mesh: regular
  // meshes and instance groups all ride one drawIndexedIndirect. The SRB is
  // bound once and the draw fans out through the indirect cmd list.
  cb.setShaderResources(&srb);

  // PIPELINE_STATE: { "VERTEX_COUNT": N, "INSTANCE_COUNT": M, "TOPOLOGY": ... }
  // is a procedural draw override: issue one cb.draw(N, M, 0, 0) and ignore the
  // incoming geometry's index and indirect buffers, letting the vertex shader
  // drive positions from gl_VertexIndex and gl_InstanceIndex. Used by
  // fullscreen passes (skybox, VERTEX_COUNT=3) and procedural geometry (VSA
  // plasma, VERTEX_COUNT=10000, TOPOLOGY=line_strip).
  //
  // When the shader declares non-empty VERTEX_INPUTS it reads vertex
  // attributes, so the draw count is clamped to the incoming geometry's
  // vertex_count. A shader living purely on gl_VertexIndex should declare
  // VERTEX_INPUTS: [], which builds the pipeline with no vertex bindings and
  // uses VERTEX_COUNT verbatim.
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

      // Bind vertex buffers from the geometry's `input` list, not from every
      // entry in m_meshbufs.buffers: the scene preprocessor appends the index
      // buffer and the scene-wide SSBOs to g.buffers for the auxiliary mapping,
      // and binding those as vertex buffers trips
      // VUID-vkCmdBindVertexBuffers-pBuffers-00627. g.input is authoritative.
      //
      // Which of those inputs, and in what order, is the plan's business:
      // the pipeline's layout was compacted to the streams the shader
      // reads, so slot k is g.input[plan.mesh_bindings[k]]. Skipping an
      // input on a null handle would shift every slot after it onto the
      // wrong stream, so an incomplete set binds nothing at all -- which
      // is what this path already did when no handle resolved.
      QVarLengthArray<QRhiCommandBuffer::VertexInput, 8> inputs;
      bool inputsOk = true;
      if(this->geometry.meshes && !this->geometry.meshes->meshes.empty())
      {
        const auto& g0 = this->geometry.meshes->meshes[0];
        const auto slotCount
            = plan.compacted ? plan.mesh_bindings.size() : g0.input.size();
        for(std::size_t k = 0; k < slotCount && inputsOk; ++k)
        {
          const std::size_t in_idx
              = plan.compacted ? (std::size_t)plan.mesh_bindings[k] : k;
          if(in_idx >= g0.input.size())
          {
            inputsOk = false;
            break;
          }
          const auto& in = g0.input[in_idx];
          const std::size_t idx = (std::size_t)in.buffer;
          QRhiBuffer* h
              = idx < m_meshbufs.buffers.size() ? m_meshbufs.buffers[idx].handle
                                                : nullptr;
          if(!h)
          {
            inputsOk = false;
            break;
          }
          inputs.push_back({h, (quint32)in.byte_offset});
        }
      }
      if(!inputsOk)
        inputs.clear();
      if(!inputs.isEmpty())
        cb.setVertexInput(0, inputs.size(), inputs.data());

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
    // Plan-aware draw: the pipeline was built for a compacted binding
    // set, and/or the shader declared "REQUIRED: false" VERTEX_INPUTS
    // whose semantics are missing from upstream geometry. Plain
    // pass-through otherwise (zero overhead when the plan is empty).
    if(!plan.empty())
    {
      if(auto* cm2 = dynamic_cast<const CustomMesh*>(m_mesh))
        cm2->drawWithFallbackBindings(m_meshbufs, cb, plan);
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

// Generic integer-expression evaluator, shared by EXECUTION_MODEL=MANUAL
// (COUNT) and OUTPUTS.WIDTH / HEIGHT, with a pure-integer fast path for the
// common literal case. The variable surface matches CSF dispatch expressions:
// $WIDTH / $HEIGHT / $DEPTH / $LAYERS of the first input image, unsuffixed and
// per-name, plus scalar input values as $<inputName>; '$' rewrites to 'var_'.
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
  // ossia::math_expression::add_constant stores a double& into `data`, so the
  // reserve must cover every emplace_back below: a realloc past capacity
  // dangles every previously registered reference. Upper bound: up to 4 doubles
  // per image input plus 4 one-time, 1 per scalar input, and 2 per INPUTS
  // storage/uniform and per top-level AUXILIARY. 6*inputs covers the inputs and
  // the fixed 16 absorbs the one-time set.
  {
    const auto& desc0 = n.descriptor();
    data.reserve(16 + 6 * desc0.inputs.size() + 2 * desc0.auxiliary.size());
  }

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

  // Register $COUNT_<bufferName> / $BYTESIZE_<bufferName> for every SSBO and UBO
  // the pipeline binds. Same semantics as CSF: COUNT is the flexible array's
  // element count, or 1 for UBOs and fixed-layout SSBOs; BYTESIZE is the raw
  // byte size. Lets OUTPUTS.WIDTH / HEIGHT and MANUAL-count expressions size
  // themselves against upstream buffer extents by name.
  //
  // Live sizes come from m_auxiliarySSBOs, layout from the descriptor,
  // cross-referenced by name.
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
