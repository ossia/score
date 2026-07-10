#include <Gfx/Graph/RenderedISFSamplerUtils.hpp>
#include <Gfx/Graph/RenderedVSANode.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>

namespace score::gfx
{

static const constexpr auto quad_vertex_shader = R"_(#version 450
layout(location = 0) in vec2 position;
out gl_PerVertex { vec4 gl_Position; };

void main() {
  gl_Position = vec4(position, 0.0, 1.);
}
)_";

static const constexpr auto quad_fragment_shader = R"_(#version 450
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform bgcolor_t {
  vec4 color;
} bgcolor;
void main() {
  fragColor = bgcolor.color;
}
)_";
SimpleRenderedVSANode::SimpleRenderedVSANode(const ISFNode& node) noexcept
    : score::gfx::NodeRenderer{node}
    , n{const_cast<ISFNode&>(node)}
{
}

void SimpleRenderedVSANode::updateInputTexture(const Port& input, QRhiTexture* tex, QRhiTexture* depthTex)
{
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

  if(sampler_idx < (int)m_inputSamplers.size())
  {
    auto& sampl = m_inputSamplers[sampler_idx];
    if(sampl.texture != tex)
    {
      sampl.texture = tex;
      for(auto& pd : m_passes)
        if(pd.main_pass.p.srb)
          score::gfx::replaceTexture(*pd.main_pass.p.srb, sampl.sampler, tex);
    }

    if(depthTex
       && (input.flags & Flag::SamplableDepth) == Flag::SamplableDepth
       && sampler_idx + 1 < (int)m_inputSamplers.size())
    {
      auto& depthSampl = m_inputSamplers[sampler_idx + 1];
      if(depthSampl.texture != depthTex)
      {
        depthSampl.texture = depthTex;
        for(auto& pd : m_passes)
          if(pd.main_pass.p.srb)
            score::gfx::replaceTexture(*pd.main_pass.p.srb, depthSampl.sampler, depthTex);
      }
    }
  }
}

std::vector<Sampler> SimpleRenderedVSANode::allSamplers() const noexcept
{
  // Input ports
  std::vector<Sampler> samplers = m_inputSamplers;

  // Audio textures
  samplers.insert(samplers.end(), m_audioSamplers.begin(), m_audioSamplers.end());

  return samplers;
}

void SimpleRenderedVSANode::initPass(
    const TextureRenderTarget& renderTarget, RenderList& renderer, Edge& edge,
    QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;

  // Background color quad pass
  static const auto& bg_mesh = PlainTriangle::instance();
  QRhiGraphicsPipeline* bg_pip = rhi.newGraphicsPipeline();
  QRhiBuffer* bg_ubo
      = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 4 * sizeof(float));
  bg_ubo->setName("SimpleRenderedVSANode::bg_ubo");
  QRhiShaderResourceBindings* bg_srb = rhi.newShaderResourceBindings();
  MeshBuffers bg_tri;
  {
    bg_ubo->create();
    bg_srb->setBindings({QRhiShaderResourceBinding::uniformBuffer(
        0, QRhiShaderResourceBinding::FragmentStage, bg_ubo)});
    bg_srb->create();

    auto [v, f] = score::gfx::makeShaders(
        renderer.state, quad_vertex_shader, quad_fragment_shader);
    bg_pip->setShaderStages(
        {{QRhiShaderStage::Vertex, v}, {QRhiShaderStage::Fragment, f}});
    bg_pip->setRenderPassDescriptor(renderTarget.renderPass);
    bg_pip->setDepthTest(false);
    bg_pip->setDepthWrite(false);
    // Use the actual RT sample count to stay in sync with the render target
    {
      const int rtS = renderTarget.sampleCount();
      bg_pip->setSampleCount(rtS > 0 ? rtS : renderer.samples());
    }
    bg_mesh.preparePipeline(*bg_pip);
    bg_tri = renderer.initMeshBuffer(bg_mesh, res);
    bg_pip->setRenderPassDescriptor(renderTarget.renderPass);
    bg_pip->setShaderResourceBindings(bg_srb);

    SCORE_ASSERT(bg_pip->create());
  }

  // Main rendering
  auto& model_passes = n.descriptor().passes;
  SCORE_ASSERT(model_passes.size() == 1);

  QRhiBuffer* pubo{};
  pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("SimpleRenderedVSANode::initPass::pubo");
  pubo->create();

  // The background-pass objects (bg_pip/bg_srb/bg_ubo) were already created
  // above and are only ever adopted into m_passes on the success path below.
  // Every failure exit from the main-pass build (ps->create() failing, or any
  // exception out of makeShaders / SCORE_ASSERT) must release them, otherwise
  // they leak on each addOutputPass — e.g. a TriangleFan primitive on D3D11,
  // re-triggered on every render-target-spec change. bg_tri is NOT released:
  // its buffers are cached/owned by RenderList::m_vertexBuffers (shared).
  auto releaseBackground = [&] {
    delete bg_pip;
    delete bg_srb;
    delete bg_ubo;
  };

  // Create the main pass.
  // Apply cull-mode, front-face, and blend state BEFORE the first create()
  // call so we only compile the PSO once instead of the previous two-compile
  // pattern (buildPipeline::create + destroy + mutate + create).
  QRhiGraphicsPipeline* ps = nullptr;
  QRhiShaderResourceBindings* srb = nullptr;
  try
  {
    auto [v, s] = score::gfx::makeShaders(renderer.state, n.m_vertexS, n.m_fragmentS);
    srb = score::gfx::createDefaultBindings(
        renderer, renderTarget, pubo, m_materialUBO, allSamplers());

    // Inline the essential steps of buildPipeline(srb) so we can insert the
    // VSA-specific cull/front-face/blend state before create().
    ps = rhi.newGraphicsPipeline();
    SCORE_ASSERT(ps);
    ps->setName("SimpleRenderedVSANode::initPass::ps");

    // VSA blend: simple alpha blend (no premul factors needed here).
    QRhiGraphicsPipeline::TargetBlend t{};
    t.enable = true;
    ps->setTargetBlends({t});

    const int rtS = renderTarget.sampleCount();
    ps->setSampleCount(rtS > 0 ? rtS : renderer.samples());

    m_mesh->preparePipeline(*ps);

    // INVARIANT: VSA (Vertex Shader Art) draws are NEVER face-culled — they
    // MUST use CullMode::None on every backend.
    //
    // This MUST run AFTER m_mesh->preparePipeline() above, because
    // BasicMesh::preparePipeline() unconditionally calls setCullMode()/
    // setFrontFace() (Mesh.cpp:47-49); we override its result here.
    //
    // Why None (and why a per-backend cull can NEVER be consistent here):
    // face-culling is decided from the triangle's *window-space* winding
    // sign, which QRhi does NOT normalise across backends. It stays
    // consistent ONLY for shaders that follow the QRhi convention, i.e. that
    // multiply gl_Position by QRhi::clipSpaceCorrMatrix() and do NOT flip Y
    // on SPIRV/Vulkan — that is what the consistent paths do (ISF blit_vs in
    // libisf isf.cpp:44, RenderedRawRasterPipelineNode, the RGBA decoder),
    // all of which then cull with a single CullMode::Back.
    //
    // VSA does the OPPOSITE (libisf isf.cpp:5620): it skips
    // clipSpaceCorrMatrix and instead manually does `gl_Position.y = -y` on
    // SPIRV/HLSL/MSL. That keeps the rendered image ORIENTATION consistent
    // across backends, but it INVERTS the window-space winding sign on
    // Vulkan relative to OpenGL (GL: identity corr + Y-up framebuffer;
    // Vulkan: manual Y-flip + Y-down, positive-height viewport). The upshot,
    // verified against the L3 matrix: for ANY single triangle winding,
    // exactly one of GL/Vulkan keeps the face and the other culls it — so no
    // per-backend CullMode + FrontFace combination can make one
    // front-facing VSA triangle visible on both. (Front on GL / Back on
    // Vulkan, as tried before, still diverged.)
    //
    // VSA art is 2-D procedural geometry driven purely by gl_VertexIndex;
    // "front vs back face" is not a meaningful notion for it. Drawing both
    // faces (None) is the only choice that is visible AND identical on every
    // backend. Points/line VSA modes are unaffected either way (only
    // triangles/polygons are ever culled).
    ps->setCullMode(QRhiGraphicsPipeline::CullMode::None);

    if(!renderer.anyNodeRequiresDepth())
    {
      ps->setDepthTest(false);
      ps->setDepthWrite(false);
    }

    ps->setShaderStages(
        {{QRhiShaderStage::Vertex, v}, {QRhiShaderStage::Fragment, s}});
    ps->setShaderResourceBindings(srb);
    SCORE_ASSERT(renderTarget.renderPass);
    ps->setRenderPassDescriptor(renderTarget.renderPass);

    Pipeline pip{};
    if(ps->create())
    {
      pip = {ps, srb};
      m_passes.emplace_back(
          &edge, Pass{renderTarget, pip, pubo}, bg_pip, bg_srb, bg_ubo, bg_tri);
    }
    else
    {
      qDebug() << "Warning! VSA pipeline not created";
      delete ps;
      delete srb;
      delete pubo;
      releaseBackground();
    }
  }
  catch(...)
  {
    // makeShaders / SCORE_ASSERT(renderTarget.renderPass) etc. can throw after
    // some of the objects were created: release everything that is not owned by
    // an m_passes entry (the success path is the only one that adopts them).
    delete ps;
    delete srb;
    delete pubo;
    releaseBackground();
  }
}

void SimpleRenderedVSANode::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  initState(renderer, res);

  for(Edge* edge : n.output[0]->edges)
    addOutputPass(renderer, *edge, res);
}

void SimpleRenderedVSANode::initState(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;

  m_prevFormat = *(int*)n.input.back()->value;
  // Create the mesh
  {
    auto m = new DummyMesh(3); // Set in update
    switch(m_prevFormat)
    {
      case 0:
        m->topology = QRhiGraphicsPipeline::Topology::Points;
        break;
      case 1:
        m->topology = QRhiGraphicsPipeline::Topology::LineStrip;
        break;
      case 2:
      case 3:
        m->topology = QRhiGraphicsPipeline::Topology::Lines;
        break;
      case 4:
        m->topology = QRhiGraphicsPipeline::Topology::TriangleStrip;
        break;
      case 5:
        m->topology = QRhiGraphicsPipeline::Topology::TriangleFan;
        break;
      case 6:
        m->topology = QRhiGraphicsPipeline::Topology::Triangles;
        break;
    }

    m_mesh = m;
  }

  // Create the material UBO
  m_materialSize = n.m_materialSize;
  if(m_materialSize > 0)
  {
    m_materialUBO
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
    m_materialUBO->setName("SimpleRenderedVSANode::init::m_materialUBO");
    SCORE_ASSERT(m_materialUBO->create());
    if(n.m_material_data)
      res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, n.m_material_data.get());
  }

  // Create the samplers
  SCORE_ASSERT(m_passes.empty());
  SCORE_ASSERT(m_inputSamplers.empty());
  SCORE_ASSERT(m_audioSamplers.empty());

  m_inputSamplers = initInputSamplers(this->n, renderer, n.input, &n.descriptor());

  m_audioSamplers = initAudioTextures(renderer, n.m_audio_textures);

  m_initialized = true;
}

void SimpleRenderedVSANode::addOutputPass(
    RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res)
{
  auto rt = renderer.renderTargetForOutput(edge);
  if(rt.renderTarget)
  {
    initPass(rt, renderer, edge, res);
  }
}

void SimpleRenderedVSANode::removeOutputPass(RenderList& renderer, Edge& edge)
{
  auto it
      = ossia::find_if(m_passes, [&](const auto& p) { return p.edge == &edge; });
  if(it != m_passes.end())
  {
    it->main_pass.p.release();

    if(it->main_pass.processUBO)
      it->main_pass.processUBO->deleteLater();

    it->background_pipeline->destroy();
    it->background_pipeline->deleteLater();

    it->background_srb->destroy();
    it->background_srb->deleteLater();

    it->background_ubo->destroy();
    it->background_ubo->deleteLater();

    m_passes.erase(it);
  }
}

bool SimpleRenderedVSANode::hasOutputPassForEdge(Edge& edge) const
{
  return ossia::find_if(m_passes, [&](const auto& p) { return p.edge == &edge; })
         != m_passes.end();
}

void SimpleRenderedVSANode::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
{
  SCORE_ASSERT(n.input.size() >= 2);
  const auto primitiveType = *(int*)n.input.back()->value;
  {
    if(primitiveType != m_prevFormat)
    {
      release(renderer);
      init(renderer, res);
    }
  }

  const auto count = *(float*)n.input[n.input.size() - 2]->value;
  static_cast<DummyMesh*>(m_mesh)->vertexCount = count;

  n.standardUBO.passIndex = 0;
  n.standardUBO.frameIndex++;
  auto sz = renderer.renderSize(edge);
  n.standardUBO.renderSize[0] = sz.width();
  n.standardUBO.renderSize[1] = sz.height();

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
      QRhiTexture* boundTex = tex ? tex : &renderer.emptyTexture();

      // Keep m_audioSamplers[i].texture in sync with the live GPU texture.
      // If a pass is later torn down and rebuilt (e.g. rt_changed path in
      // RenderList::render calling removeOutputPass + addOutputPass),
      // allSamplers() must hand buildPipeline the current texture so the
      // fresh SRB is bound correctly. Without this sync the rebuilt SRB
      // would bind &renderer.emptyTexture() (because m_audioSamplers had
      // texture=nullptr from initAudioTextures) and no subsequent
      // updateAudioTexture would ever re-trigger replaceTexture — the
      // post-no-change path returns {} — so the shader would read zero
      // for the rest of the session. Observed as 1×1 empty texture in
      // RenderDoc after a viewport resize.
      if(audio_idx < m_audioSamplers.size())
        m_audioSamplers[audio_idx].texture = tex;

      for(auto& pass : m_passes)
      {
        score::gfx::replaceTexture(
            *pass.main_pass.p.srb, rhiSampler, boundTex);
      }
    }
    ++audio_idx;
  }

  // Update material
  if(m_materialUBO && m_materialSize > 0 && (materialChanged || audioChanged))
  {
    char* data = n.m_material_data.get();
    res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
  }
  materialChanged = false;

  // Update all the process UBOs
  for(auto& pass : m_passes)
  {
    float color[4]{
        (float)n.m_descriptor.background_color[0],
        (float)n.m_descriptor.background_color[1],
        (float)n.m_descriptor.background_color[2],
        (float)n.m_descriptor.background_color[3]};
    // FIXME handle dynamic background uniform
    res.updateDynamicBuffer(pass.background_ubo, 0, 4 * sizeof(float), color);
    res.updateDynamicBuffer(
        pass.main_pass.processUBO, 0, sizeof(ProcessUBO), &this->n.standardUBO);
  }
}

void SimpleRenderedVSANode::release(RenderList& r)
{
  releaseState(r);
}

void SimpleRenderedVSANode::releaseState(RenderList& r)
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
        it->second.texture = nullptr;
        it->second = {};
      }
    }

    for(auto& pass : m_passes)
    {
      pass.main_pass.p.release();

      if(pass.main_pass.processUBO)
        pass.main_pass.processUBO->deleteLater();

      pass.background_pipeline->destroy();
      pass.background_pipeline->deleteLater();

      pass.background_srb->destroy();
      pass.background_srb->deleteLater();

      pass.background_ubo->destroy();
      pass.background_ubo->deleteLater();
    }

    m_passes.clear();
  }

  for(auto sampler : m_inputSamplers)
  {
    delete sampler.sampler;
  }
  m_inputSamplers.clear();
  for(auto sampler : m_audioSamplers)
  {
    delete sampler.sampler;
  }
  m_audioSamplers.clear();

  delete m_materialUBO;
  m_materialUBO = nullptr;

  delete m_mesh;
  m_mesh = nullptr;

  m_initialized = false;
}

void SimpleRenderedVSANode::runInitialPasses(
    RenderList& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& updateBatch,
    Edge& edge)
{
}

void SimpleRenderedVSANode::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  auto it = ossia::find_if(this->m_passes, [&](auto& p) { return p.edge == &edge; });
  // Maybe the shader could not be created
  if(it == this->m_passes.end())
    return;

  auto& pass = it->main_pass;
  auto texture = pass.renderTarget.texture;

  // Draw the background color unless its alpha == 0
  if(n.m_descriptor.background_color[3] > 0.0)
  {
    auto pipeline = it->background_pipeline;

    cb.setGraphicsPipeline(pipeline);
    cb.setShaderResources(it->background_srb);
    cb.setViewport(
        QRhiViewport(0, 0, texture->pixelSize().width(), texture->pixelSize().height()));

    const QRhiCommandBuffer::VertexInput bindings[]
        = {{it->background_tri.buffers[0].handle, 0}};

    cb.setVertexInput(0, 1, bindings, 0);
    cb.draw(3);
  }

  // Draw the main pass
  {
    SCORE_ASSERT(pass.renderTarget.renderTarget);
    SCORE_ASSERT(pass.p.pipeline);
    SCORE_ASSERT(pass.p.srb);
    // TODO : combine all the uniforms..

    auto pipeline = pass.p.pipeline;
    auto srb = pass.p.srb;

    // TODO need to free stuff
    {
      cb.setGraphicsPipeline(pipeline);
      cb.setShaderResources(srb);
      cb.setViewport(QRhiViewport(
          0, 0, texture->pixelSize().width(), texture->pixelSize().height()));

      m_mesh->draw({}, cb);
    }
  }
}

SimpleRenderedVSANode::~SimpleRenderedVSANode() { }

}
