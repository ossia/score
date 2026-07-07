#include <Gfx/Graph/CustomMesh.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <score/tools/Debug.hpp>

#include <QDebug>
#include <QMatrix4x4>
#include <QQuaternion>

#include <ossia/detail/algorithms.hpp>

namespace score::gfx
{

TextureRenderTarget NodeRenderer::renderTargetForInput(const Port& p)
{
  return {};
}

void NodeRenderer::initState(RenderList&, QRhiResourceUpdateBatch&) { }

void NodeRenderer::releaseState(RenderList&) { }

void NodeRenderer::addOutputPass(RenderList&, Edge&, QRhiResourceUpdateBatch&) { }

void NodeRenderer::updateInputSamplerFilter(
    const Port& input, const RenderTargetSpecs& spec)
{
  // Default: no-op. Renderers that cache samplers should override.
}

void NodeRenderer::addInputEdge(RenderList&, Edge&, QRhiResourceUpdateBatch&) { }

// When an upstream edge is removed (e.g. the user inserts a Transform3D in
// the middle of an existing glTF → ScenePreprocessor wire), drop the cached
// per-(port, source) entry this edge was populating. Without this, the
// last scene/geometry pushed by the now-disconnected producer lingers in
// m_portScenes / m_portGeometries forever and rebuildMergedScene keeps
// merging it in — the user saw the "scene doesn't disappear until
// stop/start" symptom. Also wipe the merge cache so the next merge runs
// fresh.
void NodeRenderer::removeInputEdge(RenderList&, Edge& edge)
{
  if(!edge.sink || !edge.sink->node)
    return;

  // Figure out which input port of the sink this edge was landing on.
  const auto& inputs = edge.sink->node->input;
  int32_t port = -1;
  for(std::size_t i = 0; i < inputs.size(); ++i)
  {
    if(inputs[i] == edge.sink)
    {
      port = (int32_t)i;
      break;
    }
  }
  if(port < 0)
    return;

  const void* source_key = edge.source;
  const PortSourceKey key{port, source_key};

  m_portGeometries.erase(key);
  m_portScenes.erase(key);
  m_wrapCache.erase(key);

  // Also drop the legacy nullptr-keyed slot in case this edge was the sole
  // contributor via the 2-arg process() path.
  const PortSourceKey legacyKey{port, nullptr};
  m_portGeometries.erase(legacyKey);
  m_portScenes.erase(legacyKey);
  m_wrapCache.erase(legacyKey);

  // Force rebuildMergedScene to recompute from scratch next time.
  m_mergeCacheInputs.clear();
  m_mergeCacheOutput = {};
}

bool NodeRenderer::hasOutputPassForEdge(Edge& edge) const { return false; }

void NodeRenderer::seedInitialOutputs(RenderList&) { }

void defaultPassesInit(
    PassMap& passes, const std::vector<Edge*>& edges, RenderList& renderer,
    const Mesh& mesh, const QShader& v, const QShader& f, QRhiBuffer* processUBO,
    QRhiBuffer* matUBO, std::span<const Sampler> samplers,
    std::span<QRhiShaderResourceBinding> additionalBindings)
{
  SCORE_ASSERT(passes.empty());
  for(Edge* edge : edges)
  {
    auto rt = renderer.renderTargetForOutput(*edge);
    if(rt.renderTarget)
    {
      auto pip = score::gfx::buildPipeline(
          renderer, mesh, v, f, rt, processUBO, matUBO, samplers, additionalBindings);
      if(pip.pipeline)
        passes.emplace_back(edge, Pass{rt, pip, nullptr});
    }
  }
}

void defaultRenderPass(
    RenderList& renderer, const Mesh& mesh, const MeshBuffers& bufs,
    QRhiCommandBuffer& cb, Edge& edge, PassMap& passes)
{
  auto it
      = ossia::find_if(passes, [ptr = &edge](const auto& p) { return p.first == ptr; });
  if(it != passes.end())
  {
    const auto sz = renderer.renderSize(&edge);
    cb.setGraphicsPipeline(it->second.p.pipeline);
    cb.setShaderResources(it->second.p.srb);
    cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

    mesh.draw(bufs, cb);
  }
  else
  {
    qDebug() << "Could not find matching pipeline for draw";
  }
}

void quadRenderPass(
    RenderList& renderer, const MeshBuffers& bufs, QRhiCommandBuffer& cb, Edge& edge,
    PassMap& passes)
{
  auto it
      = ossia::find_if(passes, [ptr = &edge](const auto& p) { return p.first == ptr; });
  if(it == passes.end())
    return;
  {
    const auto sz = renderer.renderSize(&edge);
    cb.setGraphicsPipeline(it->second.p.pipeline);
    cb.setShaderResources(it->second.p.srb);
    cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

    const auto& mesh = renderer.defaultQuad();
    mesh.draw(bufs, cb);
  }
}

void GenericNodeRenderer::defaultMeshInit(
    RenderList& renderer, const Mesh& mesh, QRhiResourceUpdateBatch& res)
{
  m_mesh = &mesh;
  if(m_meshbufs.buffers.empty())
  {
    m_meshbufs = renderer.initMeshBuffer(mesh, res);
  }
}

void GenericNodeRenderer::processUBOInit(RenderList& renderer)
{
  auto& rhi = *renderer.state.rhi;
  m_processUBO = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  m_processUBO->setName("GenericNodeRenderer::m_processUBO");
  m_processUBO->create();
}

void GenericNodeRenderer::defaultPassesInit(RenderList& renderer, const Mesh& mesh)
{
  if(this->node.output[0]->type == score::gfx::Types::Image)
  {
    score::gfx::defaultPassesInit(
        m_p, this->node.output[0]->edges, renderer, mesh, m_vertexS, m_fragmentS,
        m_processUBO, m_material.buffer, m_samplers);
  }
}

void GenericNodeRenderer::defaultPassesInit(
    RenderList& renderer, const Mesh& mesh, const QShader& v, const QShader& f,
    std::span<QRhiShaderResourceBinding> additionalBindings)
{
  if(this->node.output[0]->type == score::gfx::Types::Image)
  {
    score::gfx::defaultPassesInit(
        m_p, this->node.output[0]->edges, renderer, mesh, v, f, m_processUBO,
        m_material.buffer, m_samplers, additionalBindings);
  }
}

void GenericNodeRenderer::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  initState(renderer, res);

  for(Edge* edge : this->node.output[0]->edges)
    addOutputPass(renderer, *edge, res);
}

void GenericNodeRenderer::initState(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  m_mesh = &renderer.defaultTriangle();
  auto& mesh = *m_mesh;
  defaultMeshInit(renderer, mesh, res);
  processUBOInit(renderer);

  m_material.init(renderer, node.input, m_samplers);
  // Upload initial material data
  if(m_material.buffer && m_material.size > 0)
  {
    auto& n = static_cast<const score::gfx::NodeModel&>(this->node);
    if(n.m_materialData)
      res.updateDynamicBuffer(m_material.buffer, 0, m_material.size, n.m_materialData.get());
  }

  m_initialized = true;
}

void GenericNodeRenderer::addOutputPass(
    RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res)
{
  if(!m_mesh)
    return;
  if(this->node.output[0]->type != score::gfx::Types::Image)
    return;

  auto rt = renderer.renderTargetForOutput(edge);
  if(!rt.renderTarget)
    return;

  // Every edge gets its own SRB. Layout is identical across edges
  // (same node, same sampler count, same UBOs) so the SRBs are all
  // layout-compatible — a requirement for sharing a pipeline built
  // against any one of them.
  auto* srb = score::gfx::createDefaultBindings(
      renderer, rt, m_processUBO, m_material.buffer, m_samplers);
  if(!srb)
    return;

  // Reuse an existing pipeline when this renderer already has one built
  // against a compatible renderpass layout. serializedFormat() is QRhi's
  // documented in-memory compatibility key (identical ⇔ isCompatible),
  // which both makes the reuse valid on Vulkan/D3D12/Metal and avoids the
  // pointer-ABA hazard of keying on the rp-desc address.
  const QVector<quint32> rpFormat = rt.renderPass->serializedFormat();
  QRhiGraphicsPipeline* pipeline = nullptr;
  for(auto& [desc, pipe] : m_pipelineCache)
  {
    if(desc == rpFormat && pipe)
    {
      pipeline = pipe;
      break;
    }
  }

  if(!pipeline)
  {
    auto pip = score::gfx::buildPipeline(
        renderer, *m_mesh, m_vertexS, m_fragmentS, rt, srb);
    if(!pip.pipeline)
    {
      srb->deleteLater();
      return;
    }
    pipeline = pip.pipeline;
    m_pipelineCache.emplace_back(rpFormat, pipeline);
  }

  // Pass::p.pipeline is non-owning here — the cache owns it. removeOutputPass
  // and releaseState null-out pipeline before Pipeline::release() so the
  // Pass release path only destroys the SRB.
  m_p.emplace_back(&edge, Pass{rt, Pipeline{pipeline, srb}, nullptr});
}

void GenericNodeRenderer::removeOutputPass(RenderList& renderer, Edge& edge)
{
  auto it
      = ossia::find_if(m_p, [&](const auto& p) { return p.first == &edge; });
  if(it == m_p.end())
    return;

  QRhiGraphicsPipeline* pipeline = it->second.p.pipeline;

  // Determine ownership: the pipeline is cache-owned iff an m_pipelineCache
  // entry still points to it. Passes produced by addOutputPass share
  // cache-owned pipelines; Passes produced by defaultPassesInit (ImageNode
  // and the like, which pre-date this cache) own their own pipeline.
  auto cacheIt = ossia::find_if(
      m_pipelineCache, [&](const auto& e) { return e.second == pipeline; });
  const bool cacheOwned = (cacheIt != m_pipelineCache.end());

  if(cacheOwned)
  {
    // Detach so Pipeline::release() won't deleteLater() the cached
    // pipeline. The SRB is still per-edge and gets dropped normally.
    it->second.p.pipeline = nullptr;
  }
  it->second.release();
  m_p.erase(it);

  if(!cacheOwned || !pipeline)
    return;

  // If no other Pass still references this cached pipeline, evict it.
  // Otherwise long-lived renderers would accumulate one cache entry per
  // historical rp-desc pointer until releaseState.
  for(const auto& entry : m_p)
  {
    if(entry.second.p.pipeline == pipeline)
      return; // still in use — leave the cache entry alone
  }
  pipeline->deleteLater();
  m_pipelineCache.erase(cacheIt);
}

bool GenericNodeRenderer::hasOutputPassForEdge(Edge& edge) const
{
  return ossia::find_if(m_p, [&](const auto& p) { return p.first == &edge; })
         != m_p.end();
}

void GenericNodeRenderer::releaseState(RenderList& renderer)
{
  if(!m_initialized)
    return;

  // Release any remaining passes. Pipelines stored in m_pipelineCache
  // are owned by the renderer itself and must NOT be deleteLater'd via
  // Pipeline::release(); any Pass whose p.pipeline is cache-owned gets
  // its pipeline zeroed out first so the Pass only drops its SRB.
  // Passes whose pipeline is NOT in the cache (produced by
  // defaultPassesInit — see ImageNode::PreloadedRenderer) retain the
  // original owning release semantics.
  for(auto& pass : m_p)
  {
    auto* pipeline = pass.second.p.pipeline;
    if(pipeline)
    {
      const bool cacheOwned = ossia::any_of(
          m_pipelineCache, [&](const auto& e) { return e.second == pipeline; });
      if(cacheOwned)
        pass.second.p.pipeline = nullptr;
    }
    pass.second.release();
  }
  m_p.clear();

  // Now destroy the cached pipelines.
  for(auto& [desc, pipeline] : m_pipelineCache)
  {
    if(pipeline)
      pipeline->deleteLater();
  }
  m_pipelineCache.clear();

  for(auto sampler : m_samplers)
  {
    delete sampler.sampler;
    // texture is deleted elsewhere
  }
  m_samplers.clear();

  delete m_processUBO;
  m_processUBO = nullptr;

  delete m_material.buffer;
  m_material.buffer = nullptr;

  // FIXME Check that they get released?
  // We should have a refcount for this
  m_meshbufs = {};

  m_initialized = false;
}

void GenericNodeRenderer::defaultUBOUpdate(
    RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  auto& n = static_cast<const score::gfx::NodeModel&>(this->node);
  res.updateDynamicBuffer(m_processUBO, 0, sizeof(ProcessUBO), &n.standardUBO);

  if(m_material.buffer && m_material.size > 0)
  {
    if(materialChanged)
    {
      char* data = n.m_materialData.get();
      res.updateDynamicBuffer(m_material.buffer, 0, m_material.size, data);
    }
    materialChanged = false;
  }
}

void GenericNodeRenderer::defaultMeshUpdate(
    RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  // 2 things to separate:
  // - A mesh's geometry can change because it's a kind of dynamic mesh
  //   which gets changed live
  // - The mesh did not change but we must update it here
  //   because e.g. the connected node changed
  // Uploaded meshes must be stored in the renderer.
  // We know they can be freed when shared_ptr of mesh goes to zero

  // Note: idea for maintaining consistency between engine and UI thread:
  // adding markers that indicate the messages for a frame.
  // e.g. special message "frame 2353 messages start .... 2353 end" and a variable that indicates
  // the last fully written frame so that wwith peek() we can check that we are going to get only
  // the messages relevant for a frame.
  // Or... just put all of one frame's message in one vector and push that one at the end of the audio frame.

  if(geometryChanged && geometry.meshes)
  {
    std::tie(m_mesh, m_meshbufs)
        = renderer.acquireMesh(geometry, res, m_mesh, m_meshbufs);
  }
}

void GenericNodeRenderer::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, score::gfx::Edge* e)
{
  defaultMeshUpdate(renderer, res);
  defaultUBOUpdate(renderer, res);
}

void GenericNodeRenderer::defaultRelease(RenderList&)
{
  // Mirror the ownership handling in releaseState — cache-owned pipelines
  // are destroyed by the cache, not by Pipeline::release().
  for(auto& pass : m_p)
  {
    auto* pipeline = pass.second.p.pipeline;
    if(pipeline)
    {
      const bool cacheOwned = ossia::any_of(
          m_pipelineCache, [&](const auto& e) { return e.second == pipeline; });
      if(cacheOwned)
        pass.second.p.pipeline = nullptr;
    }
    pass.second.release();
  }
  m_p.clear();

  for(auto& [desc, pipeline] : m_pipelineCache)
  {
    if(pipeline)
      pipeline->deleteLater();
  }
  m_pipelineCache.clear();

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

void NodeRenderer::runInitialPasses(
    RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res, Edge& e)
{
}

void NodeRenderer::runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) {
}

// Rebuild `this->scene` as the merge of every m_portScenes entry,
// memoized on the set of input scene_state pointers. When unchanged, the
// previous merged scene_spec (and its scene_state shared_ptr) is reused
// verbatim — which is what lets downstream consumers like
// ScenePreprocessorNode keep their version/pointer caches hot instead of
// re-decoding textures and re-uploading vertex/index buffers per frame.
void NodeRenderer::rebuildMergedScene()
{
  ossia::small_vector<MergeCacheKey, 4> sig;
  ossia::small_vector<const ossia::scene_spec*, 4> valid;
  for(auto& kv : m_portScenes)
  {
    const auto& s = kv.second;
    // Drop the `!s.state->empty()` filter: env-only producers
    // (EnvironmentLoader, CubemapLoader, …) have an empty roots vector
    // but still contribute environment fields — dropping them here
    // would make their skybox / ambient / fog updates invisible. Empty
    // roots are handled gracefully by the downstream merge.
    if(s.state)
    {
      sig.push_back({s.state.get(), s.state->version});
      valid.push_back(&s);
    }
  }

  if(sig == m_mergeCacheInputs && m_mergeCacheOutput.state)
  {
    this->scene = m_mergeCacheOutput;
    return;
  }
  m_mergeCacheInputs.assign(sig.begin(), sig.end());

  if(valid.empty())
  {
    this->scene = {};
    m_mergeCacheOutput = {};
    return;
  }
  if(valid.size() == 1)
  {
    this->scene = *valid[0];
    m_mergeCacheOutput = this->scene;
    return;
  }

  ossia::small_vector<ossia::scene_spec, 4> input_copies;
  input_copies.reserve(valid.size());
  for(auto* s : valid)
    input_copies.push_back(*s);
  this->scene
      = ossia::merge_scenes(std::span<const ossia::scene_spec>{
          input_copies.data(), input_copies.size()});
  m_mergeCacheOutput = this->scene;
}

void NodeRenderer::process(int32_t port, const ossia::geometry_spec& v)
{
  process(port, v, nullptr);
}

void NodeRenderer::process(
    int32_t port, const ossia::geometry_spec& v, const void* source_key)
{
  const PortSourceKey key{port, source_key};

  // Store per-(port,source) for multi-geometry-port nodes (CSF) and for
  // multi-producer accumulation on the same port.
  m_portGeometries[key] = v;

  // Backward compat: keep the single geometry field updated
  // (used by GenericNodeRenderer, RenderedRawRasterPipelineNode, etc.)
  if(this->geometry != v)
  {
    this->geometry = v;
    geometryChanged = true;
  }
  else if(this->geometry.meshes)
  {
    for(auto& mesh : this->geometry.meshes->meshes)
    {
      for(auto& buf : mesh.buffers)
      {
        if(buf.dirty)
        {
          geometryChanged = true;
          break;
        }
      }
      if(geometryChanged)
        break;
    }
  }

  // Auto-wrap into scene for scene-aware renderers. The wrap is cached
  // per (port,source) keyed on the geometry_spec identity: if the same
  // spec is re-pushed (common case — glTF / FBX loaders re-publish every
  // frame even when nothing changed) the wrapper's scene_state shared_ptr
  // stays stable across frames, which is what the merge memoization
  // relies on.
  auto& cache_entry = m_wrapCache[key];
  if(cache_entry.first != v || !cache_entry.second.state)
  {
    cache_entry.first = v;
    cache_entry.second = ossia::wrap_geometry_as_scene(v);
  }
  m_portScenes[key] = cache_entry.second;
  sceneChanged = true;
  rebuildMergedScene();
}

void NodeRenderer::process(int32_t port, const ossia::scene_spec& v)
{
  process(port, v, nullptr);
}

void NodeRenderer::process(
    int32_t port, const ossia::scene_spec& v, const void* source_key)
{
  const PortSourceKey key{port, source_key};
  m_portScenes[key] = v;
  sceneChanged = true;
  rebuildMergedScene();

  // For backward compatibility: extract the first geometry from the scene
  // so that renderers that only understand geometry_spec still work.
  auto geom = ossia::extract_first_geometry(v);
  if(geom)
  {
    m_portGeometries[key] = geom;
    if(this->geometry != geom)
    {
      this->geometry = geom;
      geometryChanged = true;
    }
  }
}

void NodeRenderer::process(int32_t port, const ossia::transform3d& v)
{
  // Apply the matrix transform to the last root node in the scene.
  // Geometry is always pushed before transform for the same edge.
  // We wrap the last root's children under a scene_transform payload.
  if(!this->scene.state || this->scene.state->empty())
    return;

  // Convert matrix-based transform3d to TRS scene_transform.
  // The matrix is column-major (from QMatrix4x4::data()).
  QMatrix4x4 mat(v.matrix, 4, 4);
  QVector3D translation = mat.column(3).toVector3D();

  // Extract rotation (assumes no shear)
  QVector3D col0 = mat.column(0).toVector3D();
  QVector3D col1 = mat.column(1).toVector3D();
  QVector3D col2 = mat.column(2).toVector3D();
  QVector3D scale(col0.length(), col1.length(), col2.length());

  QMatrix3x3 rotMat;
  if(scale.x() > 0.f) col0 /= scale.x();
  if(scale.y() > 0.f) col1 /= scale.y();
  if(scale.z() > 0.f) col2 /= scale.z();
  float rot3x3[9] = {
      col0.x(), col1.x(), col2.x(),
      col0.y(), col1.y(), col2.y(),
      col0.z(), col1.z(), col2.z()};
  QQuaternion quat = QQuaternion::fromRotationMatrix(QMatrix3x3(rot3x3));

  ossia::scene_transform xform;
  xform.translation[0] = translation.x();
  xform.translation[1] = translation.y();
  xform.translation[2] = translation.z();
  xform.rotation[0] = quat.x();
  xform.rotation[1] = quat.y();
  xform.rotation[2] = quat.z();
  xform.rotation[3] = quat.scalar();
  xform.scale[0] = scale.x();
  xform.scale[1] = scale.y();
  xform.scale[2] = scale.z();

  // Rebuild: wrap the last root under a new parent with [transform, old_root]
  auto new_roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  for(auto& root : *this->scene.state->roots)
    new_roots->push_back(root);

  if(!new_roots->empty())
  {
    auto& last_root = new_roots->back();
    if(last_root)
    {
      auto new_children = std::make_shared<std::vector<ossia::scene_payload>>();
      new_children->push_back(xform);
      // Carry over original children
      if(last_root->has_children())
        for(auto& child : *last_root->children)
          new_children->push_back(child);

      auto new_node = std::make_shared<ossia::scene_node>();
      new_node->id = last_root->id;
      new_node->children = std::move(new_children);
      new_roots->back() = std::move(new_node);
    }
  }

  auto new_state = std::make_shared<ossia::scene_state>();
  new_state->roots = std::move(new_roots);
  if(this->scene.state->materials)
    new_state->materials = this->scene.state->materials;
  if(this->scene.state->animations)
    new_state->animations = this->scene.state->animations;

  this->scene.state = std::move(new_state);
  // transform3d mutates the merged scene in place; republish it on the
  // (port, nullptr) slot since there's no single upstream producer identity
  // for the transformed result.
  m_portScenes[PortSourceKey{port, nullptr}] = this->scene;
  sceneChanged = true;
}

void GenericNodeRenderer::defaultRenderPass(
    RenderList& renderer, const Mesh& mesh, QRhiCommandBuffer& cb, Edge& edge)
{
  defaultRenderPass(renderer, mesh, cb, edge, m_p);
}

void GenericNodeRenderer::defaultRenderPass(
    RenderList& renderer, const Mesh& mesh, QRhiCommandBuffer& cb, Edge& edge,
    PassMap& passes)
{
  score::gfx::defaultRenderPass(renderer, mesh, m_meshbufs, cb, edge, passes);
}

void GenericNodeRenderer::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  const auto& mesh = renderer.defaultTriangle();
  defaultRenderPass(renderer, mesh, cb, edge);
}

void GenericNodeRenderer::updateInputTexture(const Port& input, QRhiTexture* tex, QRhiTexture* depthTex)
{
  int sampler_idx = 0;
  for(auto* p : node.input)
  {
    if(p == &input)
      break;
    if(p->type == Types::Image)
    {
      sampler_idx++;
      // Skip the depth sampler that follows ports with SamplableDepth
      if((p->flags & Flag::SamplableDepth) == Flag::SamplableDepth)
        sampler_idx++;
    }
  }

  if(sampler_idx < (int)m_samplers.size())
  {
    auto& sampl = m_samplers[sampler_idx];
    if(sampl.texture != tex)
    {
      sampl.texture = tex;
      for(auto& [e, pass] : m_p)
        if(pass.p.srb)
          score::gfx::replaceTexture(*pass.p.srb, sampl.sampler, tex);
    }

    // Update the depth sampler if the port has SamplableDepth
    if(depthTex
       && (input.flags & Flag::SamplableDepth) == Flag::SamplableDepth
       && sampler_idx + 1 < (int)m_samplers.size())
    {
      auto& depthSampl = m_samplers[sampler_idx + 1];
      if(depthSampl.texture != depthTex)
      {
        depthSampl.texture = depthTex;
        for(auto& [e, pass] : m_p)
          if(pass.p.srb)
            score::gfx::replaceTexture(*pass.p.srb, depthSampl.sampler, depthTex);
      }
    }
  }
}

void GenericNodeRenderer::release(RenderList& r)
{
  releaseState(r);
}

score::gfx::NodeRenderer::~NodeRenderer() { }

BufferView NodeRenderer::bufferForInput(const Port& input)
{
  return {};
}

BufferView NodeRenderer::bufferForOutput(const Port& output)
{
  return {};
}

QRhiTexture* NodeRenderer::textureForOutput(const Port& output)
{
  return nullptr;
}

void NodeRenderer::updateInputTexture(const Port& input, QRhiTexture* tex, QRhiTexture* depthTex)
{
}

void NodeRenderer::inputAboutToFinish(
    RenderList& renderer, const Port& p, QRhiResourceUpdateBatch*&)
{
}


}
