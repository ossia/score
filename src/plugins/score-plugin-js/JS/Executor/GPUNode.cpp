#include "GPUNode.hpp"

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <JS/Qml/QmlObjects.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickGraphicsDevice>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>

#include <private/qquickrendercontrol_p.h>
#include <private/qquickwindow_p.h>
#include <private/qsgdefaultrendercontext_p.h>
namespace JS
{

struct GpuNode : score::gfx::NodeModel
{
public:
  explicit GpuNode(const QString& source);
  virtual ~GpuNode();

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;

  void process(score::gfx::Message&& msg) override;

  QString source;
};

class GpuRenderer : public score::gfx::GenericNodeRenderer
{
public:
  const GpuNode& node;
  explicit GpuRenderer(const GpuNode& node) noexcept
      : score::gfx::GenericNodeRenderer{node}
      , node{node}
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
  vec2 texcoordAdjust;
  vec2 renderSize;
};

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = clipSpaceCorrMatrix * vec4(position, 0.0, 1.);
}
)_";

  static const constexpr auto fragment_shader = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;
  vec2 renderSize;
};

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

  void createItem()
  {
    qDebug().noquote().nospace() << "Created! " << node.source.toUtf8();
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

    if(auto texout = m_object->findChild<TextureOutlet*>(); texout != nullptr)
    {
      this->m_item = texout->item();
      if(this->m_item)
      {
        this->m_item->setParent(this->m_window->contentItem());
        this->m_item->setParentItem(this->m_window->contentItem());
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
    // m_loop = new QEventLoop{};
    m_renderControl = new QQuickRenderControl{};
    m_window = new QQuickWindow{m_renderControl};
    m_window->setGraphicsDevice(QQuickGraphicsDevice::fromRhi(renderer.state.rhi));

    m_window->setWidth(renderer.state.renderSize.width());
    m_window->setHeight(renderer.state.renderSize.height());
    m_window->setColor(Qt::transparent);

    m_renderControl->initialize();
    m_window->setRenderTarget(
        QQuickRenderTarget::fromRhiRenderTarget(m_internalTex.renderTarget));

    m_engine = new QQmlEngine{};
    createItem();
  }

  void update(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override {
    defaultUBOUpdate(renderer, res);
  }

  QElapsedTimer t;
  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& e) override
  {
    t.restart();
    // Here we run the Qt Qucik render loop which handles its own pass
    // 1. Run our object

    auto& tick = m_object->tick();
    if(tick.isCallable())
    {
      // 1. Copy the values in the inputs
      score::gfx::Message msg;
      while(m_messages.try_dequeue(msg))
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

      // 2. Run
      tick.call();
    }

    // 2. Render
    auto cd = QQuickWindowPrivate::get(m_window);
    auto rc = QQuickRenderControlPrivate::get(m_renderControl);
    rc->cb = &cb;

    cd->deliveryAgentPrivate()->flushFrameSynchronousEvents(m_window);
    cd->polishItems();
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

    m_engine->collectGarbage();

    // cb.endPass(); // < called by renderSceneGraph
    res = renderer.state.rhi->nextResourceUpdateBatch();

    t.restart();
  }

  void runRenderPass(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      score::gfx::Edge& edge) override
  {
    const auto& mesh = renderer.defaultQuad();
    defaultRenderPass(renderer, mesh, cb, edge);
  }

  void release(score::gfx::RenderList& r) override
  {
    m_ctrlInlets.clear();
    m_impulseInlets.clear();
    m_valInlets.clear();
    m_jsInlets.clear();

    delete m_object;
    m_object = nullptr;

    delete m_component;
    m_component = nullptr;

    delete m_window;
    m_window = nullptr;

    delete m_renderControl;
    m_renderControl = nullptr;

    delete m_engine;
    m_engine = nullptr;

    delete m_loop;
    m_loop = nullptr;

    m_internalTex.release();

    defaultRelease(r);
  }

  score::gfx::TextureRenderTarget m_internalTex;
  QEventLoop* m_loop{};
  QQmlComponent* m_component{};
  JS::Script* m_object{};
  QQuickItem* m_item{};

  QQmlEngine* m_engine{};
  QQuickRenderControl* m_renderControl{};
  QQuickWindow* m_window{};

  std::vector<Inlet*> m_jsInlets;
  std::vector<std::pair<ControlInlet*, int>> m_ctrlInlets;
  std::vector<std::pair<Impulse*, int>> m_impulseInlets;
  std::vector<std::pair<ValueInlet*, int>> m_valInlets;

  QJSValueList m_tickCall;

  ossia::spsc_queue<score::gfx::Message> m_messages;
  friend struct GpuNode;
};

GpuNode::GpuNode(const QString& source)
    : source{source}
{
  output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
}
GpuNode::~GpuNode() { }

score::gfx::NodeRenderer*
GpuNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  return new GpuRenderer{*this};
}

void GpuNode::process(score::gfx::Message&& msg)
{
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
  exec_context->ui->unregister_node(id);

  auto n = std::make_unique<JS::GpuNode>(str);

  id = exec_context->ui->register_node(std::move(n));
}
}
#endif
