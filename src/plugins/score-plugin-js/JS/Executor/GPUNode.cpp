#include "GPUNode.hpp"

#if defined(SCORE_HAS_GPU_JS)
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <JS/Qml/QmlObjects.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <boost/unordered/concurrent_flat_map.hpp>

#include <QQmlComponent>
#include <QQmlEngine>
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

struct GpuNode : score::gfx::NodeModel
{
public:
  explicit GpuNode(const QString& source);
  virtual ~GpuNode();

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;

  void process(score::gfx::Message&& msg) override;

  QString source;
  std::atomic_int64_t sourceIndex{};

  score::gfx::Message m_lastState;

  struct Engine
  {
    QQmlEngine* m_engine{};
    QQmlComponent* m_component{};
    JS::Script* m_object{};
    QQuickItem* m_item{};

    std::vector<Inlet*> m_jsInlets;
    std::vector<std::pair<ControlInlet*, int>> m_ctrlInlets;
    std::vector<std::pair<Impulse*, int>> m_impulseInlets;
    std::vector<std::pair<ValueInlet*, int>> m_valInlets;

    void init(GpuNode& node, QQuickWindow* window)
    {
      if(!m_item)
      {
        if(!m_engine)
        {
          m_engine = new QQmlEngine{};
        }
        createItem(node);
      }

      updateItemTextureOut(window);
    }

    void createItem(GpuNode& node)
    {
      m_component = new QQmlComponent{this->m_engine};
      m_component->setData(node.source.toUtf8(), QUrl{});
      auto obj = m_component->create();
      m_object = qobject_cast<JS::Script*>(obj);
      if(!m_object)
      {
        delete obj;
        return;
      }

      setupComponent();
    }

    void updateItemTextureOut(QQuickWindow* window)
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

    void setupComponent()
    {
      // FIXME refactor with CPUNode
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
      }
    }

    void releaseItem()
    {
      if(m_item)
      {
        m_item->setParent(nullptr);
        m_item->setParentItem(nullptr);
      }
    }

    ~Engine()
    {
      m_ctrlInlets.clear();
      m_impulseInlets.clear();
      m_valInlets.clear();
      m_jsInlets.clear();

      delete m_object;
      m_object = nullptr;

      delete m_component;
      m_component = nullptr;

      delete m_engine;
      m_engine = nullptr;
    }

    void processMessage(const score::gfx::Message& msg)
    {
      for(std::size_t i = 0; i < msg.input.size(); i++)
      {
        if(auto it = ossia::get_if<ossia::value>(&msg.input[i]))
        {
          auto var = it->apply(ossia::qt::ossia_to_qvariant{});

          SCORE_ASSERT(m_jsInlets.size() > i);
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

    void tick()
    {
      if(auto& tick = m_object->tick(); tick.isCallable())
      {
        tick.call();
      }
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
  fragColor = texture(y_tex, vec2(v_texcoord.x, 1. - v_texcoord.y));
}
)_";

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    return {};
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& rhi = *renderer.state.rhi;

    // Init the texture on which we are going to render
    m_internalTex = score::gfx::createRenderTarget(
        renderer.state, QRhiTexture::RGBA32F, renderer.state.renderSize,
        renderer.state.samples);

    // Init basic rendering ubos
    const auto& mesh = renderer.defaultQuad();
    defaultMeshInit(renderer, mesh, res);
    processUBOInit(renderer);
    std::tie(m_vertexS, m_fragmentS)
        = score::gfx::makeShaders(renderer.state, vertex_shader, fragment_shader);

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
    m_window->setGraphicsDevice(QQuickGraphicsDevice::fromRhi(&rhi));

    m_window->setWidth(renderer.state.renderSize.width());
    m_window->setHeight(renderer.state.renderSize.height());
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
        m_engine->init(node, m_window);
      }
    }
  }

  void update(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override {
    reloadEngine(renderer.state.rhi);
    defaultUBOUpdate(renderer, res);
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

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& e) override
  {
    // Here we run the Qt Qucik render loop which handles its own pass

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
    cb.beginPass(rt, QColor(Qt::transparent), {}, res);
    res = nullptr;

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
    // cb.endPass(); // < called by renderSceneGraph
    res = renderer.state.rhi->nextResourceUpdateBatch();
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

GpuNode::GpuNode(const QString& source)
    : source{source}
    , sourceIndex{1}
{
  output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
}
GpuNode::~GpuNode() { }

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

gpu_exec_node::gpu_exec_node(Gfx::GfxExecutionAction& ctx)
    : gfx_exec_node{ctx}
{
}

gpu_exec_node::~gpu_exec_node()
{
  if(id >= 0)
    exec_context->ui->unregister_node(id);
}

std::string gpu_exec_node::label() const noexcept
{
  return "JS::gpu_exec_node";
}

void gpu_exec_node::setScript(const QString& str)
{
  // exec_context->ui->unregister_node(id);

  if(id < 0)
  {
    auto n = std::make_unique<JS::GpuNode>(str);

    id = exec_context->ui->register_node(std::move(n));
  }
  else
  {
    auto msg = exec_context->allocateMessage(1);
    msg.node_id = id;
    msg.input.emplace_back(score::gfx::FunctionMessage{[str](score::gfx::Node& nn) {
      auto& n = static_cast<GpuNode&>(nn);
      n.source = str; // FIXME mutex
      n.sourceIndex++;
    }});
    exec_context->ui->send_message(std::move(msg));
  }
}
}
#endif
