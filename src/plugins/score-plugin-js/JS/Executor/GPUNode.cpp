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

#include <compare>
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
      QObject* uiContext,
      JS::JSState&& st,
      const QString& source,
      const ossia::inlets& ins,
      const ossia::outlets& outs);
  virtual ~GpuNode();

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;

  void process(score::gfx::Message&& msg) override;

  QPointer<QObject> m_uiContext;
  JS::JSState m_modelState;
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
    QQuickItem* m_item{};

    std::vector<Inlet*> m_jsInlets;
    std::vector<std::pair<ControlInlet*, int>> m_ctrlInlets;
    std::vector<std::pair<Impulse*, int>> m_impulseInlets;
    std::vector<std::pair<ValueInlet*, int>> m_valInlets;
    std::vector<std::pair<TextureInlet*, int>> m_texInlets;

    ossia::spsc_queue<js_message_type> ui_messages;

    void init(GpuRenderer& renderer, GpuNode& node, QQuickWindow* window);

    void createItem(GpuRenderer& renderer, GpuNode& node);

    void updateItemTextureOut(QQuickWindow* window);

    void setupComponent(GpuRenderer& renderer, GpuNode& node);

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
    // FIXME find if there's a more atomic way to implement this with insert_or_visit,
    // without calling init() inside the map's lock.
    std::shared_ptr<Engine> res;
    m_engines.visit(key, [&](const auto& engine) { res = engine.second; });

    if(!res)
    {
      res = std::make_shared<Engine>();
      m_engines.insert({key, res});
    }
    return {key, res};
  }

  void releaseEngine(QRhi* rhi) { m_engines.erase({std::this_thread::get_id(), rhi}); }

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
  ossia::small_flat_map<const score::gfx::Port*, score::gfx::TextureRenderTarget, 2>
      m_rts;

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    auto res = m_rts.find(&p);
    if(res != m_rts.end() && res->second)
      return res->second;
    return {};
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& rhi = *renderer.state.rhi;

    // Init the texture on which we are going to render
    // FIXME RGBA32F
    m_internalTex = score::gfx::createRenderTarget(
        renderer.state, QRhiTexture::RGBA8, renderer.state.renderSize,
        renderer.state.samples, true);

    // Init basic rendering ubos
    const auto& mesh = renderer.defaultQuad();
    defaultMeshInit(renderer, mesh, res);
    processUBOInit(renderer);
    std::tie(m_vertexS, m_fragmentS)
        = score::gfx::makeShaders(renderer.state, vertex_shader, fragment_shader);

    m_inputSamplers
        = score::gfx::initInputSamplers(this->node, renderer, this->node.input, m_rts);

    // Create the sampler in which we are going to put the texture
    {
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

      sampler->setName("JS::GPUNode::sampler");
      sampler->create();

      m_samplers.push_back({sampler, m_internalTex.texture});
    }

    defaultPassesInit(renderer, mesh);

    // Init the QQuick render stuff
    m_renderControl = new QQuickRenderControl{};
    m_window = new QQuickWindow{m_renderControl};

#if QT_HAS_VULKAN
    if(renderer.state.api == score::gfx::GraphicsApi::Vulkan)
    {
      m_window->setVulkanInstance(score::gfx::staticVulkanInstance());
    }
#endif

    if(auto win = renderer.state.window.lock())
    {
      QObject::connect(
          win.get(), &score::gfx::Window::interactiveEvent, m_window,
          [qqw = QPointer{m_window}](QEvent* e) {
        if(auto q = qqw.get())
          QCoreApplication::sendEvent(q, e);
      }, Qt::DirectConnection);
    }
    m_window->setGraphicsDevice(QQuickGraphicsDevice::fromRhi(&rhi));

    const auto sz = renderer.state.renderSize;
    m_window->setWidth(sz.width());
    m_window->setHeight(sz.height());
    m_window->contentItem()->setWidth(sz.width());
    m_window->contentItem()->setWidth(sz.height());
    m_window->setColor(Qt::transparent);

    m_renderControl->initialize();
    m_window->setRenderTarget(
        QQuickRenderTarget::fromRhiRenderTarget(m_internalTex.renderTarget));
  }

  void reloadEngine(QRhi* rhi)
  {
    auto oldSourceIndex = this->sourceIndex.exchange(this->node.sourceIndex);
    //= std::exchange(this->sourceIndex, this->node.sourceIndex.load());
    // yes technically there is the overflow case but it's 2^64 editions away...
    if(oldSourceIndex < this->node.sourceIndex)
    {
      if(m_engine)
      {
        m_engine->releaseItem();
      }

      node.releaseEngine(rhi);
      m_engine.reset();
      auto [key, engine] = node.acquireEngine(rhi);
      m_tid = key.id;
      m_engine = engine;
      if(m_engine)
      {
        m_engine->init(*this, node, m_window);

        for(auto& [texture_in, i] : this->m_engine->m_texInlets)
        {
          SCORE_ASSERT(this->node.input.size() > i);
          score::gfx::Port* port = this->node.input[i];
          SCORE_ASSERT(port->type == score::gfx::Types::Image);
          auto& rt = m_rts[port];
          auto item = qobject_cast<JS::TextureInletItem*>(texture_in->item());
          SCORE_ASSERT(item);
          item->setSize(rt.texture->pixelSize());
        }
      }
    }
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
    reloadEngine(renderer.state.rhi);
    defaultUBOUpdate(renderer, res);

    // Schedule a copy of the input textures into the actual textures
    {
      for(auto& [texture_in, i] : this->m_engine->m_texInlets)
      {
        SCORE_ASSERT(this->node.input.size() > i);
        score::gfx::Port* port = this->node.input[i];
        SCORE_ASSERT(port->type == score::gfx::Types::Image);
        auto& rt = m_rts[port];
        auto item = qobject_cast<JS::TextureInletItem*>(texture_in->item());
        SCORE_ASSERT(item);
        auto renderer = item->renderer;
        auto texture = item->texture;
        if(renderer && texture)
        {
          if(rt.texture->pixelSize() == texture->pixelSize()
             && rt.texture->sampleCount() == texture->sampleCount())
          {
            QRhiTextureCopyDescription desc;
            res.copyTexture(texture, rt.texture, desc);
          }
          else
          {
            qDebug() << "Mismatch!!!" << rt.texture->pixelSize() << texture->pixelSize()
                     << rt.texture->sampleCount() << texture->sampleCount();
          }
        }
        else
        {
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

    if(m_engine && m_engine->m_engine)
    {
      m_engine->m_engine->collectGarbage();
    }

    QEvent* updateRequest = new QEvent(QEvent::UpdateRequest);
    QCoreApplication::postEvent(m_window, updateRequest);
  }

  void runRenderPass(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      score::gfx::Edge& edge) override
  {
    const auto& mesh = renderer.defaultQuad();
    defaultRenderPass(renderer, mesh, cb, edge);
    m_window->frameSwapped();
  }

  void release(score::gfx::RenderList& r) override
  {
    if(m_engine)
    {
      m_engine->releaseItem();
    }

    for(auto [edge, rt] : m_rts)
    {
      rt.release();
    }
    m_rts.clear();

    for(auto sampler : m_inputSamplers)
    {
      delete sampler.sampler;
      // texture is deleted elsewhere
    }
    m_inputSamplers.clear();

    delete m_window;
    m_window = nullptr;

    delete m_renderControl;
    m_renderControl = nullptr;

    m_internalTex.release();

    defaultRelease(r);
  }

  score::gfx::TextureRenderTarget m_internalTex;

  QQuickRenderControl* m_renderControl{};
  QQuickWindow* m_window{};

  ossia::spsc_queue<score::gfx::Message> m_messages;
  std::thread::id m_tid;
  std::shared_ptr<GpuNode::Engine> m_engine;

  friend struct GpuNode;
};

GpuNode::GpuNode(
    QObject* uiContext,
    JS::JSState&& st,
    const QString& source, const ossia::inlets& ins, const ossia::outlets& outs)
    : m_uiContext{uiContext}
    , m_modelState{std::move(st)}
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
GpuNode::~GpuNode() { }


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
}

void GpuNode::Engine::releaseItem()
{
  if(m_item)
  {
    m_item->setParent(nullptr);
    m_item->setParentItem(nullptr);
  }
}

void GpuNode::Engine::setupComponent(GpuRenderer& renderer, GpuNode& node)
{
  // FIXME refactor with CPUNode
  // FIXME only works because same thread right now.
  // Re-read QQuickRenderControl and use it to separate
  // execution in GPUNode and rendering in GPURenderer

  QObject::connect(m_object, &JS::Script::uiSend,
                   node.m_uiContext, [this, &node] (const QJSValue& v) {
    if(!node.m_uiContext)
      return;
    QMetaObject::invokeMethod(qApp, [ctx=node.m_uiContext, &func = node.m_messageToUi, vv = v.toVariant()] {
      if(!ctx)
        return;
      func(std::move(vv));
    }, Qt::QueuedConnection);
  }, Qt::DirectConnection);

  if(const auto& on_load = m_object->loadState(); on_load.isCallable())
  {
    QVariantMap vm;
    for(auto& [k, v]: node.m_modelState) {
      if(auto res = v.apply(ossia::qt::ossia_to_qvariant{}); res.isValid())
        vm[k] = std::move(res);
    }
    on_load.call({m_engine->toScriptValue(vm)});
  }

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

void GpuNode::Engine::createItem(GpuRenderer& renderer, GpuNode& node)
{
  m_component = new QQmlComponent{this->m_engine.get()};
  m_component->setData(node.source.toUtf8(), QUrl{});
  if(m_component->isError())
  {
    qDebug() << m_component->errorString();
    return;
  }

  auto obj = m_component->create();
  m_object = qobject_cast<JS::Script*>(obj);
  if(!m_object)
  {
    delete obj;
    return;
  }

  setupComponent(renderer, node);
}

void GpuNode::Engine::init(GpuRenderer& renderer, GpuNode& node, QQuickWindow* window)
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
      m_execFuncs = new DeviceContext{*m_engine};
      m_execFuncs->init();

      m_context->setContextProperty("Device", m_execFuncs);
      setupExecFuncs(this, &node, m_execFuncs->m_impl);
    }
    createItem(renderer, node);
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

void gpu_exec_node::setScript(const QString& str, JS::JSState&& new_state)
{
  exec_context->ui->unregister_node(id);

  //if(id < 0)
  {
    auto n
        = std::make_unique<JS::GpuNode>(m_context, std::move(new_state), str, this->root_inputs(), this->root_outputs());

    {
      auto& element = *m_context;
      n->m_uiContext = m_context;
      n->m_messageToUi = [ctx=m_context] (const QVariant& v){
        OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
        if(!ctx)
          return;
        ctx->executionToUi(v);
      };

      QObject::connect(
          &element, &JS::ProcessModel::uiToExecution, n.get(),
          [node=n.get()](const QVariant& v) {
        node->uiMessage(v);
      }, Qt::QueuedConnection);
      QObject::connect(
          &element, &JS::ProcessModel::stateElementChanged, n.get(),
          [node=n.get()](const QString& k, const ossia::value& v) {
        node->stateElementChanged(k, v);
      }, Qt::QueuedConnection);

    }
    id = exec_context->ui->register_node(std::move(n));
  }
  /*
  else
  {
    // FIXME need to update the ports if they changed on the host side!
    auto msg = exec_context->allocateMessage(1);
    msg.node_id = id;
    msg.input.emplace_back(score::gfx::FunctionMessage{[str](score::gfx::Node& nn) {
      auto& n = static_cast<GpuNode&>(nn);
      n.source = str; // FIXME mutex
      n.sourceIndex++;
    }});
    exec_context->ui->send_message(std::move(msg));
  }
*/
}
}
#endif
