#pragma once
#include <Gfx/Graph/CommonUBOs.hpp>
#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Uniforms.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <ossia/dataflow/geometry_port.hpp>
#include <ossia/dataflow/nodes/media.hpp>
#include <ossia/dataflow/texture_port.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/variant.hpp>
#include <ossia/network/value/value.hpp>

#include <score_plugin_gfx_export.h>

#include <vector>

namespace score::gfx
{
class RenderList;
struct Graph;
class GenericNodeRenderer;
class NodeRenderer;
using FunctionMessage = std::function<void(score::gfx::Node&)>;
#if BOOST_VERSION < 107900
// Old boost: small_vector was not nothrow-move-constructible so we remove the check there.
using gfx_input = ossia::slow_variant<
    ossia::monostate, ossia::value, ossia::audio_vector, ossia::render_target_spec,
    ossia::geometry_spec, ossia::transform3d, FunctionMessage>;
#else
using gfx_input = ossia::variant<
    ossia::monostate, ossia::value, ossia::audio_vector, ossia::render_target_spec,
    ossia::geometry_spec, ossia::transform3d, FunctionMessage>;
#endif

/**
 * @brief Messages sent from the execution thread to the rendering thread
 *
 * The input is the array of controls of the execution node.
 */
struct Timings
{
  ossia::time_value date{};
  ossia::time_value parent_duration{};
};

struct Message
{
  int32_t node_id{};
  Timings token{};
  std::vector<gfx_input> input;
};

struct RenderTargetSpecs
{
  QSize size;

  QRhiTexture::Format format = QRhiTexture::Format ::RGBA8;

  QRhiSampler::Filter mag_filter = QRhiSampler::Linear;
  QRhiSampler::Filter min_filter = QRhiSampler::Linear;
  QRhiSampler::Filter mipmap_mode = QRhiSampler::None;

  QRhiSampler::AddressMode address_u = QRhiSampler::Repeat;
  QRhiSampler::AddressMode address_v = QRhiSampler::Repeat;
  QRhiSampler::AddressMode address_w = QRhiSampler::Repeat;
};

/**
 * @brief Root data model for visual nodes.
 */
class SCORE_PLUGIN_GFX_EXPORT Node : public QObject
{
public:
  explicit Node();
  virtual ~Node();

  Node(const Node&) = delete;
  Node(Node&&) = delete;
  Node& operator=(const Node&) = delete;
  Node& operator=(Node&&) = delete;

  /**
   * @brief Create a renderer in a given context for this node.
   */
  virtual NodeRenderer* createRenderer(RenderList& r) const noexcept = 0;

  /**
   * @brief Whenever render nodes are added / removed
   */
  virtual void renderedNodesChanged();

  /**
   * @brief Process a message from the execution engine
   */
  virtual void process(Message&& msg);
  virtual void update();

  /**
   * @brief Input ports of that node.
   */
  std::vector<Port*> input;
  /**
   * @brief Output ports of that node.
   *
   * Most of the time there will be a single image output.
   */
  ossia::small_pod_vector<Port*, 1> output;

  /**
   * @brief Map associating each RenderList to a Renderer for this model.
   */
  ossia::flat_map<RenderList*, score::gfx::NodeRenderer*> renderedNodes;

  /**
   * @brief Render target info
   * 
   * Each texture inlet will have a matching spec
   */
  ossia::flat_map<int32_t, ossia::render_target_spec> renderTargetSpecs;

  /**
   * @brief Used to notify a material change from the model to the renderers.
   */
  void materialChange() noexcept
  {
    materialChanged.fetch_add(1, std::memory_order_release);
  }
  bool hasMaterialChanged(int64_t& renderer) const noexcept
  {
    int64_t res = materialChanged.load(std::memory_order_acquire);
    if(renderer != res)
    {
      renderer = res;
      return true;
    }
    return false;
  }
  std::atomic_int64_t materialChanged{0};

  /**
   * @brief Used to notify a geometry change from the model to the renderers.
   */
  void geometryChange() noexcept
  {
    geometryChanged.fetch_add(1, std::memory_order_release);
  }
  bool hasGeometryChanged(int64_t& renderer) const noexcept
  {
    int64_t res = geometryChanged.load(std::memory_order_acquire);
    if(renderer != res)
    {
      renderer = res;
      return true;
    }
    return false;
  }
  std::atomic_int64_t geometryChanged{-1};

  /**
   * @brief Used to notify a render target (texture inlet) change from the model to the renderers.
   */
  void renderTargetChange() noexcept
  {
    renderTargetSpecChanged.fetch_add(1, std::memory_order_release);
  }
  bool hasRenderTargetChanged(int64_t& renderer) const noexcept
  {
    int64_t res = renderTargetSpecChanged.load(std::memory_order_acquire);
    if(renderer != res)
    {
      renderer = res;
      return true;
    }
    return false;
  }
  std::atomic_int64_t renderTargetSpecChanged{-1};

  int32_t id = -1;
  bool requiresDepth{};
  bool addedToGraph{};

  QSize resolveRenderTargetSize(int32_t port, RenderList& renderer) const noexcept;
  RenderTargetSpecs
  resolveRenderTargetSpecs(int32_t port, RenderList& renderer) const noexcept;

  void process(int32_t port, const ossia::render_target_spec& v);
};

/**
 * @brief Common base class for nodes that map to score processes.
 */
class SCORE_PLUGIN_GFX_EXPORT ProcessNode : public Node
{
public:
  using Node::Node;

  /**
   * @brief Every node matching with a score process will have such an UBO.
   *
   * It has useful information, such as timing, sample rate, mouse position etc.
   */
  ProcessUBO standardUBO{};

  /**
   * @brief The geometry to use
   *
   * If not set, then a relevant default geometry for the node
   * will be used, e.g. a full-screen quad or triangle
   */
  ossia::geometry_spec geometry;

  void process(Message&& msg) override;
  void process(Timings tk);
  void process(int32_t port, const ossia::value& v);
  void process(int32_t port, const ossia::audio_vector& v);
  void process(int32_t port, const ossia::geometry_spec& v);
  void process(int32_t port, const ossia::transform3d& v);
  void process(int32_t port, ossia::monostate) const noexcept { }
  void process(int32_t port, const FunctionMessage&);
  using Node::process;
};

/**
 * @brief Common base class for most single-pass, simple nodes.
 */
class SCORE_PLUGIN_GFX_EXPORT NodeModel : public score::gfx::ProcessNode
{
public:
  explicit NodeModel();
  virtual ~NodeModel();

  virtual score::gfx::NodeRenderer*
  createRenderer(RenderList& r) const noexcept override;

protected:
  std::unique_ptr<char[]> m_materialData;

  friend class GenericNodeRenderer;
};
}

// QDebug operator<<(QDebug, const score::gfx::gfx_input&);
QDebug operator<<(QDebug, const std::vector<score::gfx::gfx_input>&);
