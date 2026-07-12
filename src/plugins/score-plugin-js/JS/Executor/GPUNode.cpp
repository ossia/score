#include "GPUNode.hpp"
#include <JS/Executor/ExecutionHelpers.hpp>
#include <JS/JSProcessMetadata.hpp>

#if defined(SCORE_HAS_GPU_JS)
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Window.hpp>
#include <JS/Qml/QmlObjects.hpp>
#include <JS/Qml/QmlRhiObjects.hpp>
#include <JS/Qml/Utils.hpp>
#include <JS/ThreadLocalQmlEngine.hpp>
#include <JS/Qml/DeviceContext.hpp>

#include <score/gfx/Vulkan.hpp>

#include <ossia-qt/js_utilities.hpp>
#include <ossia-qt/qml_engine_functions.hpp>

#include <boost/unordered/concurrent_flat_map.hpp>

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickGraphicsDevice>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>

#include <private/qquickrendercontrol_p.h>
#include <private/qquickwindow_p.h>
#include <private/qsgcontext_p.h>
#include <private/qsgdefaultrendercontext_p.h>

#include <compare>
#include <set>
namespace JS
{
struct engine_key
{
  std::thread::id id;
  QRhi* rhi;
  std::strong_ordering operator<=>(const engine_key& other) const noexcept = default;
};
struct engine_key_hash
{
  std::size_t operator()(const JS::engine_key& k) const noexcept
  {
    return std::hash<std::thread::id>{}(k.id) ^ intptr_t(k.rhi);
  }
};
class GpuRenderer;
struct GpuNode : score::gfx::NodeModel
{
public:
  explicit GpuNode(
      QObject* uiContext, JS::JSState&& st, const QString& root, const QString& source,
      const ossia::inlets& ins, const ossia::outlets& outs);
  virtual ~GpuNode();

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;

  using score::gfx::NodeModel::process;
  void process(score::gfx::Message&& msg) override;

  QPointer<QObject> m_uiContext;
  JS::JSState m_modelState;
  QString m_root;
  QString source;
  std::atomic_int64_t sourceIndex{};

  score::gfx::Message m_lastState;
  std::function<void(QVariant)> m_messageToUi;

  using js_message_type = ossia::variant<QVariant, std::pair<QString, ossia::value>>;
  void uiMessage(const QVariant& v);
  void stateElementChanged(const QString& k, const ossia::value& v);

  struct Engine
  {
    std::shared_ptr<QQmlEngine> m_engine{};
    QQmlContext* m_context{};
    DeviceContext* m_execFuncs{};
    QQmlComponent* m_component{};
    JS::Script* m_object{};
    QPointer<QQuickItem> m_item{};

    // Qt Quick runtime. Created in GpuRenderer::initState(), destroyed
    // when the Engine itself is destroyed (GpuRenderer::release() drops
    // the map entry and the renderer's own shared_ptr, bringing refcount
    // to zero). Destruction runs while the owning QRhi is still alive —
    // see the note in GpuRenderer::release() for why this matters.
    QQuickRenderControl* m_quickRenderControl{};
    QQuickWindow* m_quickWindow{};

    std::vector<Inlet*> m_jsInlets;
    std::vector<std::pair<ControlInlet*, int>> m_ctrlInlets;
    std::vector<std::pair<Impulse*, int>> m_impulseInlets;
    std::vector<std::pair<ValueInlet*, int>> m_valInlets;
    std::vector<std::pair<TextureInlet*, int>> m_texInlets;

    ossia::spsc_queue<js_message_type> ui_messages;

    void init(
        GpuRenderer& renderer, GpuNode& node, QQuickWindow* window,
        score::gfx::RenderList& rl);

    void createItem(
        GpuRenderer& renderer, GpuNode& node, score::gfx::RenderList& rl);

    void updateItemTextureOut(QQuickWindow* window);

    void setupComponent(
        GpuRenderer& renderer, GpuNode& node, score::gfx::RenderList& rl);

    void releaseItem();

    ~Engine();

    void processMessage(const score::gfx::Message& msg);

    void tick();

    void uiMessage(const QVariant& v)
    {
      if(!m_object)
        return;

      const auto& on_ui = this->m_object->uiEvent();
      if(!on_ui.isCallable())
        return;

      on_ui.call({m_engine->toScriptValue(v)});
    }

    void stateElementChanged(const QString& k, const ossia::value& v)
    {
      if(!m_object)
        return;

      const auto& on_stateUpdated = this->m_object->stateUpdated();
      if(!on_stateUpdated.isCallable())
        return;

      if(v.valid())
      {
        if(auto res = v.apply(ossia::qt::ossia_to_qvariant{}); res.isValid())
          on_stateUpdated.call({k, m_engine->toScriptValue(res)});
        else
          on_stateUpdated.call({k, QJSValue{}});
      }
      else
        on_stateUpdated.call({k, QJSValue{}});
    }
  };

  std::pair<const engine_key, std::shared_ptr<Engine>> acquireEngine(QRhi* rhi)
  {
    const auto key = engine_key{std::this_thread::get_id(), rhi};
    std::shared_ptr<Engine> res;
    m_engines.try_emplace_and_visit(
        key,
        std::make_shared<Engine>(),
        [&](auto& slot) { res = slot.second; },   // newly-inserted visitor
        [&](auto& slot) { res = slot.second; });  // existing-key visitor
    return {key, res};
  }

  // Release by the key stored at acquire time, NOT by the current thread id.
  // If releaseState() ever runs on a different thread than initState()'s
  // insert (e.g. under SCORE_THREADED_GFX), erasing by the current-thread
  // key would leave the stale Engine (with m_quickWindow set) mapped, and
  // the next acquire would return it and trip the SCORE_ASSERT in initState().
  void releaseEngine(const engine_key& key) { m_engines.erase(key); }

  boost::concurrent_flat_map<engine_key, std::shared_ptr<Engine>, engine_key_hash>
      m_engines;
};

void GpuNode::uiMessage(const QVariant& v)
{
  m_engines.visit_all([&] (auto& elt) {
    elt.second->ui_messages.emplace(v);
  });
}

void GpuNode::stateElementChanged(const QString& k, const ossia::value& v)
{
  // Keep m_modelState in sync so that engine recreation loads current state
  if(v.valid())
    m_modelState[k] = v;
  else
    m_modelState.erase(k);

  m_engines.visit_all([&, p = std::make_pair(k, v)] (auto& elt) {
    elt.second->ui_messages.emplace(p);
  });
}


class GpuRenderer : public score::gfx::GenericNodeRenderer
{
public:
  GpuNode& node;
  std::atomic_int64_t sourceIndex{};
  explicit GpuRenderer(const GpuNode& node) noexcept
      : score::gfx::GenericNodeRenderer{node}
      , node{(GpuNode&)node}
  {
  }

private:
  ~GpuRenderer() { }

  static const constexpr auto vertex_shader = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(binding = 3) uniform sampler2D y_tex;
layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position, 0.0, 1.);
#if defined(QSHADER_GLSL) || defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = - gl_Position.y;
#endif
}
)_";

  static const constexpr auto fragment_shader = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(binding=3) uniform sampler2D y_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main ()
{
  fragColor = texture(y_tex, vec2(v_texcoord.x, v_texcoord.y));
}
)_";

  std::vector<score::gfx::Sampler> m_inputSamplers;

  // All setup lives in initState() rather than init(), because the
  // incremental graph-edit path (Graph::incrementalEdgeUpdate) calls
  // initState() directly on newly-spawned renderers without ever going
  // through init(). If we put setup in init(), a play/stop/play cycle
  // leaves the new GpuRenderer with empty shaders, no window, no engine,
  // and the next update() crashes in defaultUBOUpdate. Mirror
  // RenderedISFNode's split: initState() does all shared state;
  // the inherited GenericNodeRenderer::init() calls initState() then
  // addOutputPass() per output edge.
  // Ignore the base GenericNodeRenderer::updateInputTexture behavior:
  // GpuRenderer's m_samplers is a private, single-entry vector holding the
  // internal "y_tex" sampler that points at m_internalTex (the texture Qt
  // Quick renders into, which our fragment shader samples). Its 8 visible
  // texture-inlet ports are routed through m_engine->m_texInlets and the
  // per-frame res.copyTexture in update() — they are NOT meant to drive
  // m_samplers. The base implementation indexes m_samplers by image-input
  // position, so a sink-sampler update for input[0] (Image 1) writes
  // m_samplers[0].texture = image1_rt_texture and rebinds the SRB's y_tex
  // sampler away from m_internalTex, which makes the presentation render
  // Image 1's content directly instead of the Qt Quick tree. This fires
  // whenever Graph::updateAllSinkSamplers runs after initial pass
  // construction — i.e. on every live graph edit — which is the
  // "presentation reverts to Image 1" regression.
  //
  // Leaving it as a no-op is correct: sink-sampler updates targeting inlet
  // items are already handled by GpuRenderer::update's per-frame
  // copyTexture path (GPUNode.cpp:~470), which reads rt.texture fresh
  // every frame.
  void updateInputTexture(
      const score::gfx::Port& input, QRhiTexture* tex,
      QRhiTexture* depthTex = nullptr) override
  {
  }

  void initState(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& rhi = *renderer.state.rhi;
    // Init the texture on which we are going to render
    // FIXME RGBA32F
    m_internalTex = score::gfx::createRenderTarget(
        renderer.state, QRhiTexture::RGBA8, renderer.state.renderSize,
        renderer.state.samples, true);

    // Use the quad mesh (GenericNodeRenderer::initState would default to
    // triangle). The inherited addOutputPass uses m_mesh to build pipelines.
    m_mesh = &renderer.defaultQuad();
    defaultMeshInit(renderer, *m_mesh, res);
    processUBOInit(renderer);
    std::tie(m_vertexS, m_fragmentS)
        = score::gfx::makeShaders(renderer.state, vertex_shader, fragment_shader);

    m_inputSamplers
        = score::gfx::initInputSamplers(this->node, renderer, this->node.input);

    // Create the sampler in which we are going to put the texture
    {
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

      sampler->setName("JS::GPUNode::sampler");
      sampler->create();

      m_samplers.push_back({sampler, m_internalTex.texture});
    }

    // Acquire the Engine. release() drops the map entry and our own
    // ref, so we always get a fresh Engine here — tying the Qt Quick
    // runtime lifetime strictly to (initState, release) lets us free
    // all QRhi-owned buffers before the RHI itself is destroyed in
    // Graph::~Graph.
    auto [key, engine] = node.acquireEngine(&rhi);
    m_engineKey = key;
    m_engine = engine;
    if(!m_engine)
    {
      m_initialized = true;
      return;
    }

    SCORE_ASSERT(!m_engine->m_quickWindow);
    m_engine->m_quickRenderControl = new QQuickRenderControl{};
    m_engine->m_quickWindow = new QQuickWindow{m_engine->m_quickRenderControl};

#if QT_HAS_VULKAN
    if(renderer.state.api == score::gfx::GraphicsApi::Vulkan)
      m_engine->m_quickWindow->setVulkanInstance(
          score::gfx::staticVulkanInstance());
#endif

    if(auto win = renderer.state.window.lock())
    {
      QObject::connect(
          win.get(), &score::gfx::Window::interactiveEvent,
          m_engine->m_quickWindow,
          [qqw = QPointer{m_engine->m_quickWindow}](QEvent* e) {
        if(auto q = qqw.get())
          QCoreApplication::sendEvent(q, e);
      }, Qt::DirectConnection);
    }
    m_engine->m_quickWindow->setGraphicsDevice(
        QQuickGraphicsDevice::fromRhi(&rhi));
    m_engine->m_quickWindow->setColor(Qt::transparent);
    m_engine->m_quickRenderControl->initialize();
    // Mark the window as "visible" so QQuickItem::grabToImage() works.
    // The window is driven by QQuickRenderControl (no native OS
    // window) — this only sets the internal flag.
    QQuickWindowPrivate::get(m_engine->m_quickWindow)->visible = true;

    m_window = m_engine->m_quickWindow;
    m_renderControl = m_engine->m_quickRenderControl;

    // Size and render target are per-RenderList and must be refreshed
    // on every initState() (resize changes the RT dimensions).
    const auto sz = renderer.state.renderSize;
    m_window->setWidth(sz.width());
    m_window->setHeight(sz.height());
    m_window->contentItem()->setWidth(sz.width());
    m_window->contentItem()->setHeight(sz.height());
    m_window->setRenderTarget(
        QQuickRenderTarget::fromRhiRenderTarget(m_internalTex.renderTarget));

    m_engine->init(*this, node, m_window, renderer);
    // Tolerant of script/port mismatches (live-edited QML may not line up
    // with the node's declared ports): skip bad inlets instead of aborting.
    // Mirrors Engine::setupComponent's guards.
    for(auto& [texture_in, i] : this->m_engine->m_texInlets)
    {
      if(i >= (int)this->node.input.size())
        continue;
      score::gfx::Port* port = this->node.input[i];
      if(!port || port->type != score::gfx::Types::Image)
        continue;
      auto rt = renderer.renderTargetForInputPort(*port);
      auto item = qobject_cast<JS::TextureInletItem*>(texture_in->item());
      if(item && rt.texture)
        item->setSize(rt.texture->pixelSize());
    }
    sourceIndex.store(node.sourceIndex.load());
    m_initialized = true;
  }

  void reloadEngine(score::gfx::RenderList& renderer)
  {
    // Guard: initState() bails out early if Engine acquisition failed,
    // leaving m_window/m_renderControl/m_engine null. update() can still
    // be invoked in that degraded state — short-circuit here.
    if(!m_window || !m_renderControl || !m_engine)
      return;

    // NOTE: GpuNode::sourceIndex is fixed at 1 and never incremented (the
    // incrementer that drove the in-place script reload was removed), so the
    // GpuRenderer::sourceIndex seeded in initState() always equals it. The
    // mid-play "drop the QML tree, keep the QQuickWindow, re-init" reload
    // branch that used to live here was therefore dead code and has been
    // removed. A live script change currently goes through a full
    // releaseState()/initState() cycle instead.
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
    reloadEngine(renderer);
    defaultUBOUpdate(renderer, res);

    if(!m_engine)
      return;

    // Schedule a copy of the input textures into the actual textures.
    // Tolerant of script/port mismatches (live-edited QML): skip bad inlets
    // instead of asserting. Mirrors Engine::setupComponent's guards.
    {
      for(auto& [texture_in, i] : this->m_engine->m_texInlets)
      {
        if(i >= (int)this->node.input.size())
          continue;
        score::gfx::Port* port = this->node.input[i];
        if(!port || port->type != score::gfx::Types::Image)
          continue;
        auto rt = renderer.renderTargetForInputPort(*port);
        auto item = qobject_cast<JS::TextureInletItem*>(texture_in->item());
        if(!item)
          continue;
        auto itemRenderer = item->renderer;
        auto texture = item->texture;
        if(itemRenderer && texture && rt.texture)
        {
          const bool sameSize = rt.texture->pixelSize() == texture->pixelSize();
          const bool sameSamples
              = rt.texture->sampleCount() == texture->sampleCount();
          if(sameSize && sameSamples)
          {
            QRhiTextureCopyDescription desc;
            res.copyTexture(texture, rt.texture, desc);
          }
          else if(!sameSize)
          {
            // The upstream RT changed dimensions since the last initState().
            // Resize the inlet item so Qt Quick rebuilds its QSGRhiLayer at
            // the new size; this frame's copy is intentionally skipped
            // (src/dst pair is mismatched) and the next update() will copy
            // correctly once the layer texture is recreated.
            item->setSize(rt.texture->pixelSize());
          }
          else
          {
            // Size matches but sample count differs (e.g. the inlet item's
            // QSGRhiLayer is single-sampled while the upstream RT is MSAA).
            // QRhi::copyTexture requires matching sample counts, so the copy
            // can't run and setSize() is a no-op here — without a diagnostic
            // the inlet would stay silently black. We can't resolve/recreate
            // the layer at a different sample count from outside Qt Quick, so
            // the defined fallback is: skip the copy (the inlet keeps its
            // last content rather than showing undefined data) and warn once
            // per item so the condition is observable.
            if(m_warnedSampleMismatch.insert(item).second)
            {
              qWarning() << "JS::GPUNode: texture inlet" << i
                         << "sample-count mismatch (upstream"
                         << rt.texture->sampleCount() << "vs inlet"
                         << texture->sampleCount()
                         << ") - copy skipped, inlet may appear stale/black";
            }
          }
        }
      }
    }
  }

  void processMessages()
  {
    score::gfx::Message msg;
    while(m_messages.try_dequeue(msg))
    {
      if(m_engine)
        m_engine->processMessage(msg);
    }
  }

  void render() { }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& e) override
  {
    if(!m_window || !m_renderControl || !m_engine)
      return;
    // Here we run the Qt Quick render loop which handles its own pass
    if(auto sz = m_window->size(); sz != m_window->contentItem()->size())
    {
      m_window->contentItem()->setSize(sz); // why does this happen???
    }

    // 0. Copy the values in the inputs
    processMessages();

    // 1. Run our object
    if(m_engine)
    {
      m_engine->tick();
      if(m_engine->m_item)
        m_engine->m_item->update();

      // Mark texture inlet items dirty so the scene graph re-captures them
      for(auto& [texture_in, i] : m_engine->m_texInlets)
      {
        if(auto item = texture_in->item())
          item->update();
      }
    }
    // 2. Render
    m_window->beforeRendering();

    auto cd = QQuickWindowPrivate::get(m_window);
    auto rc = QQuickRenderControlPrivate::get(m_renderControl);
    rc->cb = &cb;

    cd->deliveryAgentPrivate()->flushFrameSynchronousEvents(m_window);
    cd->polishItems();
    m_window->afterRendering();
    m_window->afterAnimating();

    // beginFrame:
    m_window->beforeFrameBegin();

    // sync:
    cd->setCustomCommandBuffer(&cb);

    auto rt = renderer.renderTargetForOutput(e).renderTarget;
    SCORE_ASSERT(rt);

    cd->syncSceneGraph();
    rc->rc->endSync();
    // render:
    cd->renderSceneGraph();
    // endFrame:
    m_window->afterFrameEnd();

    // Disassociate our transient cb — Qt's own qsgrhisupport pairs
    // setCustomCommandBuffer(cb) with setCustomCommandBuffer(nullptr)
    // to avoid leaving a dangling pointer past the frame.
    cd->setCustomCommandBuffer(nullptr);
    // Symmetric reset of QQuickRenderControlPrivate::cb. The earlier
    // assignment at `rc->cb = &cb` (line ~523) bound the private field
    // to a stack reference parameter; without this nullptr reset the
    // pointer dangled into reclaimed stack memory after the frame
    // returned. Whether Qt internals dereferenced it between frames
    // depended on the QQuickRenderControlPrivate event-loop paths
    // (animation tick / glyph upload completion / sync without render),
    // but the fix is one line either way and removes the foot-gun.
    rc->cb = nullptr;

    // Force-drain Qt Quick's glyph-cache resource-update batch. The batch
    // is lazily allocated in preprocess() (storeGlyphs → createTexture →
    // glyphCacheResourceUpdates) and is normally released when a glyph
    // node renders and calls commitResourceUpdates. When the QML scene
    // has no glyph node, preprocess still populates the cache but no
    // draw ever commits → the batch stays pinned, permanently consuming
    // one slot of the 64-slot QRhi pool *per render context*. Each
    // window resize spawns a fresh QQuickRenderControl + render context,
    // so after a handful of resizes the pool exhausts and SIGSEGV lands
    // inside QSGRhiDistanceFieldGlyphCache::createTexture. Merge any
    // pending uploads into our outer batch so they still land, then
    // reset the context's pointer so the pool slot returns.
    if(auto* rcp = QQuickRenderControlPrivate::get(m_renderControl))
    {
      if(auto* defRc = qobject_cast<QSGDefaultRenderContext*>(rcp->rc))
      {
        if(auto* pending = defRc->maybeGlyphCacheResourceUpdates())
        {
          if(res)
            res->merge(pending);
          defRc->resetGlyphCacheResources();
        }
      }
    }
    if(m_engine && m_engine->m_engine)
    {
      m_engine->m_engine->collectGarbage();
    }
    // No UpdateRequest post needed: runInitialPasses drives sync/render
    // directly via polishItems/syncSceneGraph/renderSceneGraph each frame.
  }

  void runRenderPass(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      score::gfx::Edge& edge) override
  {
    const auto& mesh = renderer.defaultQuad();
    defaultRenderPass(renderer, mesh, cb, edge);
    if(m_window)
      m_window->frameSwapped();
  }

  void releaseState(score::gfx::RenderList& r) override
  {
    for(auto sampler : m_inputSamplers)
    {
      delete sampler.sampler;
      // texture is deleted elsewhere
    }
    m_inputSamplers.clear();

    // Tear down the Engine here — this is the last hook we get while
    // the QRhi is still alive. Graph::~Graph calls RenderList::release()
    // before out->destroyOutput() (which calls RenderState::destroy(),
    // killing the RHI); the GpuRenderer destructor runs later, after
    // the RHI is gone, so any QRhi-owned buffers still held by the
    // QQuickRenderControl/QQuickWindow would leak (VUID-vkDestroyDevice
    // validation fires at process exit).
    //
    // An earlier version kept the Engine alive across release+init to
    // avoid re-creating the Qt Quick scene graph on every window
    // resize, because each cycle pinned ~1 batch slot in Qt Quick's
    // response to setRenderTarget. That workaround is no longer needed:
    // the real batch-pool exhaustion was SimpleRenderedISFNode::initPass
    // leaking an unsubmitted batch per addOutputPass (fixed separately),
    // and Qt Quick's per-cycle slot churn alone doesn't exhaust the
    // 64-slot pool in practice.
    //
    // Living in releaseState() (not release()) is what lets live graph
    // edits that make this node unreachable actually free the Engine:
    // Graph::reconcileAllRenderLists calls releaseState() on orphaned
    // renderers, never release(). A previous version had the teardown
    // in release(), which meant node.releaseEngine() never ran on a
    // live disconnect — the next reconnection's acquireEngine returned
    // the stale entry with m_quickWindow already set and tripped the
    // SCORE_ASSERT in initState().
    //
    // USER-VISIBLE BEHAVIOR (known tradeoff): destroying the Engine here
    // discards the entire QML runtime — the QQmlEngine, the Script object
    // and ALL its script-side runtime state (JS variables, timers,
    // accumulated/animation state, etc.). Because releaseState()/initState()
    // run on every output resize (the render-target dimensions change), a
    // mid-performance window/output resize silently restarts the user's
    // script from scratch. Only the declared model state (node.m_modelState,
    // replayed via Script.loadState() in Engine::setupComponent) survives;
    // anything the script kept in plain JS variables is lost. This is
    // accepted for the deterministic-teardown lifetime guarantees above.
    m_window = nullptr;
    m_renderControl = nullptr;
    if(m_engine)
    {
      m_engine.reset();
      node.releaseEngine(m_engineKey);
    }

    m_internalTex.release();

    defaultRelease(r);
  }

  void release(score::gfx::RenderList& r) override { releaseState(r); }

  score::gfx::TextureRenderTarget m_internalTex;

  QQuickRenderControl* m_renderControl{};
  QQuickWindow* m_window{};

  ossia::spsc_queue<score::gfx::Message> m_messages;
  // Key under which our Engine was inserted in node.m_engines at acquire
  // time. We release by this stored key (see GpuNode::releaseEngine).
  JS::engine_key m_engineKey{};
  std::shared_ptr<GpuNode::Engine> m_engine;

  // Texture inlet items for which a sample-count mismatch has already been
  // reported, to rate-limit the warning to once per item (see update()).
  std::set<const void*> m_warnedSampleMismatch;

  friend struct GpuNode;
};

GpuNode::GpuNode(
    QObject* uiContext, JS::JSState&& st, const QString& root, const QString& source,
    const ossia::inlets& ins, const ossia::outlets& outs)
    : m_uiContext{uiContext}
    , m_modelState{std::move(st)}
    , m_root{root}
    , source{source}
    , sourceIndex{1}
{
  for(auto& p : ins)
  {
    switch(p->which())
    {
      case ossia::audio_port::which:
        input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Audio, {}});
        break;
      case ossia::value_port::which:
        input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Float, {}});
        break;
      case ossia::texture_port::which:
        input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
        break;
      case ossia::midi_port::which: // FIXME
        input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Empty, {}});
        break;
      case ossia::geometry_port::which: // FIXME
        input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
        break;
    }
  }

  for(auto& p : outs)
  {
    switch(p->which())
    {
      case ossia::audio_port::which:
        output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Audio, {}});
        break;
      case ossia::value_port::which:
        output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Float, {}});
        break;
      case ossia::texture_port::which:
        output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
        break;
      case ossia::midi_port::which: // FIXME
        output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Empty, {}});
        break;
      case ossia::geometry_port::which: // FIXME
        output.push_back(
            new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
        break;
    }
  }
}
GpuNode::~GpuNode()
{
}

void GpuNode::Engine::tick()
{
  if(!m_object)
    return;

  // Process messages that may come from UI
  {
    js_message_type m;
    while(ui_messages.try_dequeue(m)) {
      struct {
        Engine& self;
        void operator()(const QVariant& v) {
          self.uiMessage(v);
        }
        void operator()(const std::pair<QString, ossia::value>& v) {
          self.stateElementChanged(v.first, v.second);
        }
      } vis{*this};
      ossia::visit(vis, m);
    }
  }

  if(auto& tick = m_object->tick(); tick.isCallable())
  {
    tick.call();
  }
}

void GpuNode::Engine::processMessage(const score::gfx::Message& msg)
{
  for(std::size_t i = 0; i < msg.input.size(); i++)
  {
    if(auto it = ossia::get_if<ossia::value>(&msg.input[i]))
    {
      auto var = it->apply(ossia::qt::ossia_to_qvariant{});

      if(m_jsInlets.size() <= i)
        return;
      auto inl = m_jsInlets[i];
      if(auto v = qobject_cast<ValueInlet*>(inl))
      {
        v->clear();
        v->setValue(std::move(var));
      }
      else if(auto v = qobject_cast<Impulse*>(inl))
      {
        v->impulse();
      }
      else if(auto v = qobject_cast<ControlInlet*>(inl))
      {
        v->clear();
        v->setValue(std::move(var));
      }
    }
  }
}

GpuNode::Engine::~Engine()
{
  m_ctrlInlets.clear();
  m_impulseInlets.clear();
  m_valInlets.clear();
  m_jsInlets.clear();

  delete m_object;
  m_object = nullptr;

  delete m_component;
  m_component = nullptr;

  delete m_context;
  m_context = nullptr;

  m_engine = nullptr; // Not owned here!

  // Destroy the persistent Qt Quick runtime synchronously. Order matches
  // Qt's own QQuickWidget: QQuickRenderControl first (its destructor
  // calls invalidate() and deletes the QSGRenderContext), then the
  // QQuickWindow.
  delete m_quickRenderControl;
  m_quickRenderControl = nullptr;
  delete m_quickWindow;
  m_quickWindow = nullptr;
}

void GpuNode::Engine::releaseItem()
{
  if(m_item)
  {
    // LOAD-BEARING: these two detach calls must precede deleteLater().
    // The immediate caller (GpuRenderer::reloadEngine, GPUNode.cpp:419-420)
    // follows this with init(), whose QML reactive bindings and child-walkers
    // must not observe the dying item. setParentItem(nullptr) removes it from
    // contentItem->childItems() synchronously; setParent(nullptr) severs the
    // QObject ownership chain. deleteLater() then safely defers actual
    // destruction to the next event loop tick. Collapsing the two detach
    // calls into deleteLater() alone would briefly expose two items under
    // contentItem to the new createItem(), breaking the scene graph.
    m_item->setParent(nullptr);
    m_item->setParentItem(nullptr);
    m_item->deleteLater();
    m_item = nullptr;
  }
  // A script reload destroys the whole QML tree. Clear the script-
  // associated state here so Engine::init()'s `if(!m_item)` rebuild
  // path can recreate everything cleanly without leaking the old
  // component/object or appending to the inlet vectors.
  delete m_object;
  m_object = nullptr;
  delete m_component;
  m_component = nullptr;
  m_jsInlets.clear();
  m_ctrlInlets.clear();
  m_impulseInlets.clear();
  m_valInlets.clear();
  m_texInlets.clear();
}

void GpuNode::Engine::setupComponent(
    GpuRenderer& renderer, GpuNode& node, score::gfx::RenderList& rl)
{
  // FIXME refactor with CPUNode
  // FIXME only works because same thread right now.
  // Re-read QQuickRenderControl and use it to separate
  // execution in GPUNode and rendering in GPURenderer

  QObject::connect(
      m_object, &JS::Script::uiSend, node.m_uiContext, [&node](const QJSValue& v) {
    if(!node.m_uiContext)
      return;
    QMetaObject::invokeMethod(qApp, [ctx=node.m_uiContext, &func = node.m_messageToUi, vv = v.toVariant()] {
      if(!ctx)
        return;
      func(std::move(vv));
    }, Qt::QueuedConnection);
  }, Qt::DirectConnection);

  // (1) Enumerate QML children into the typed inlet vectors FIRST. loadState()
  //     below fires reactive bindings like `ShaderEffectSource.sourceItem =
  //     root.inletItems[src]`; those need each inlet item to already be at its
  //     final pixel size so QQuickShaderEffectSource::updatePaintNode
  //     (qquickshadereffectsource.cpp:657-664) does not take the "source item
  //     is 0x0, delete paint node, return nullptr" branch on the first sync.
  int input_i = 0;
  for(auto n : m_object->children())
  {
    if(auto imp_in = qobject_cast<Impulse*>(n))
    {
      m_jsInlets.push_back(imp_in);
      m_impulseInlets.push_back({imp_in, input_i++});
    }
    else if(auto ctrl_in = qobject_cast<ControlInlet*>(n))
    {
      m_jsInlets.push_back(ctrl_in);
      m_ctrlInlets.push_back({ctrl_in, input_i++});
    }
    else if(auto val_in = qobject_cast<ValueInlet*>(n))
    {
      m_jsInlets.push_back(val_in);
      m_valInlets.push_back({val_in, input_i++});
    }
    else if(auto tex_in = qobject_cast<TextureInlet*>(n))
    {
      m_jsInlets.push_back(tex_in);
      m_texInlets.push_back({tex_in, input_i++});
    }
    else if(auto unknown = qobject_cast<Inlet*>(n))
    {
      m_jsInlets.push_back(unknown);
      input_i++;
    }
  }

  // (2) Size each texture-inlet item to its upstream RT's pixel size BEFORE
  //     loadState runs. QML's Component.onCompleted has already rebound each
  //     inlet item's width/height to inletContainer.width/.height via
  //     Qt.binding (presentation.qml:50-53), and inletContainer is 0x0 at
  //     this point because outputRoot hasn't been reparented to contentItem
  //     yet (updateItemTextureOut runs after this). Setting the size
  //     explicitly breaks that binding and pins each item to the RT pixel
  //     size — which is exactly what the copyTexture(rt.texture ->
  //     item->texture) in GpuRenderer::update requires anyway (that copy is
  //     skipped on any pixelSize mismatch — GPUNode.cpp:456-466).
  for(auto& [texture_in, i] : m_texInlets)
  {
    if(i >= (int)node.input.size())
      continue;
    score::gfx::Port* port = node.input[i];
    if(!port || port->type != score::gfx::Types::Image)
      continue;
    auto rt = rl.renderTargetForInputPort(*port);
    auto* item = qobject_cast<JS::TextureInletItem*>(texture_in->item());
    if(item && rt.texture)
      item->setSize(rt.texture->pixelSize());
  }

  // (3) Now run loadState. Every ShaderEffectSource that resolves its
  //     sourceItem to an inletItem during the stateVersion++ re-binding pass
  //     will see a non-zero-sized source item and the first scene-graph sync
  //     will create its QSGRhiLayer (qsgrhilayer.cpp:248-254 "!m_item ||
  //     m_pixelSize.isEmpty()" branch is avoided).
  if(const auto& on_load = m_object->loadState(); on_load.isCallable())
  {
    QVariantMap vm;
    for(auto& [k, v]: node.m_modelState) {
      if(auto res = v.apply(ossia::qt::ossia_to_qvariant{}); res.isValid())
        vm[k] = std::move(res);
    }
    on_load.call({m_engine->toScriptValue(vm)});
  }
}

void GpuNode::Engine::updateItemTextureOut(QQuickWindow* window)
{
  if(!m_object)
    return;
  if(auto texout = m_object->findChild<TextureOutlet*>(); texout != nullptr)
  {
    this->m_item = texout->item();
    auto contentItem = window->contentItem();

    if(this->m_item && contentItem)
    {
      this->m_item->setParent(contentItem);
      this->m_item->setParentItem(contentItem);
    }
  }
}

void GpuNode::Engine::createItem(
    GpuRenderer& renderer, GpuNode& node, score::gfx::RenderList& rl)
{
  m_component = new QQmlComponent{this->m_engine.get()};

  m_component->setData(node.source.toUtf8(), QUrl::fromLocalFile(node.m_root));
  if(m_component->isError())
  {
    qWarning() << m_component->errorString();
    return;
  }

  auto obj = m_component->create();
  m_object = qobject_cast<JS::Script*>(obj);
  if(!m_object)
  {
    delete obj;
    return;
  }

  setupComponent(renderer, node, rl);
}

void GpuNode::Engine::init(
    GpuRenderer& renderer, GpuNode& node, QQuickWindow* window,
    score::gfx::RenderList& rl)
{
  if(!m_item)
  {
    if(!m_engine)
    {
      m_engine = JS::acquireThreadLocalEngine([](QQmlEngine& newEngine) {
        const auto& paths = score::AppContext().settings<Library::Settings::Model>().getIncludePaths();
        for(auto& path : paths) {
          newEngine.addImportPath(path);
        }

        newEngine.rootContext()->setContextProperty("Util", new JsUtils);
      });
    }
    if(!m_context)
    {
      m_context = new QQmlContext{m_engine.get()};
      m_execFuncs = new DeviceContext{*m_engine, m_context};
      m_execFuncs->init();

      m_context->setContextProperty("Device", m_execFuncs);
      setupExecFuncs(this, &node, m_execFuncs->m_impl);
    }
    createItem(renderer, node, rl);
  }

  updateItemTextureOut(window);
}

score::gfx::NodeRenderer*
GpuNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  auto res = new GpuRenderer{*this};
  res->m_messages.enqueue(m_lastState);
  return res;
}

void GpuNode::process(score::gfx::Message&& msg)
{
  if(msg.input.size() == 1 && get_if<score::gfx::FunctionMessage>(&msg.input[0]))
  {
    // This changes the script
    auto& fun = *get_if<score::gfx::FunctionMessage>(&msg.input[0]);
    fun(*this);
  }
  else
  {
    m_lastState.node_id = msg.node_id;
    m_lastState.token = msg.token;

    m_lastState.input.resize(msg.input.size());
    for(int i = 0, N = msg.input.size(); i < N; i++)
    {
      if(!get_if<ossia::monostate>(&msg.input[i]))
        m_lastState.input[i] = msg.input[i];
    }
  }

  for(auto renderer : renderedNodes)
  {
    auto r = safe_cast<GpuRenderer*>(renderer.second);
    r->m_messages.enqueue(msg);
  }
}

gpu_exec_node::gpu_exec_node(JS::ProcessModel* context, Gfx::GfxExecutionAction& ctx)
    : gfx_exec_node{ctx}
    , m_context{context}
{
}

gpu_exec_node::~gpu_exec_node()
{
  exec_context->ui->unregister_node(id);
}

std::string gpu_exec_node::label() const noexcept
{
  return "JS::gpu_exec_node";
}

void gpu_exec_node::setScript(
    const QString& root, const QString& str, JS::JSState&& new_state)
{
  exec_context->ui->unregister_node(id);
  id = score::gfx::invalid_node_index;

  auto n = std::make_unique<JS::GpuNode>(
      m_context, std::move(new_state), root, str, this->root_inputs(),
      this->root_outputs());

  {
    auto& element = *m_context;

    n->moveToThread(m_context->thread());
    n->m_uiContext = m_context;
    n->m_messageToUi = [ctx=m_context] (const QVariant& v){
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
      if(!ctx)
        return;
      ctx->executionToUi(v);
    };

    QObject::connect(
        &element, &JS::ProcessModel::uiToExecution, n.get(), &JS::GpuNode::uiMessage);
    QObject::connect(
        &element, &JS::ProcessModel::stateElementChanged, n.get(),
        &JS::GpuNode::stateElementChanged);
    {

      int i = 0;
      for(auto& ctl : element.inlets())
      {
        if(auto ctrl = qobject_cast<Gfx::TextureInlet*>(ctl))
        {
          ossia::texture_inlet& inl
              = static_cast<ossia::texture_inlet&>(*root_inputs()[i]);
          n->process(i, inl.data); // Setup render_target_spec
          // FIXME this should be done at a more general level, right now it's only done here
          // and in avendish nodes
        }
        i++;
      }
    }
  }
  id = exec_context->ui->register_node(std::move(n));
}
}
#endif
