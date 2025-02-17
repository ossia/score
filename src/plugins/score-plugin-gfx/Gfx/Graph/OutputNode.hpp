#pragma once
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Uniforms.hpp>

#include <score_plugin_gfx_export.h>
namespace score::gfx
{
class SCORE_PLUGIN_GFX_EXPORT OutputNodeRenderer : public score::gfx::NodeRenderer
{
public:
  using score::gfx::NodeRenderer::NodeRenderer;
  virtual ~OutputNodeRenderer();
  virtual void
  finishFrame(RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res);
};

class Window;
/**
 * @brief Base class for sink nodes (QWindow, spout, syphon, NDI output, ...)
 */
class SCORE_PLUGIN_GFX_EXPORT OutputNode : public score::gfx::Node
{
public:
  virtual ~OutputNode();

  virtual void setRenderer(std::shared_ptr<RenderList>) = 0;
  virtual RenderList* renderer() const = 0;

  OutputNodeRenderer* createRenderer(RenderList& r) const noexcept override = 0;

  virtual void startRendering() = 0;
  virtual void render() = 0;
  virtual void stopRendering() = 0;
  virtual bool canRender() const = 0;
  virtual void onRendererChange() = 0;

  virtual void createOutput(
      GraphicsApi graphicsApi, std::function<void()> onReady,
      std::function<void()> onUpdate, std::function<void()> onResize)
      = 0;

  virtual void updateGraphicsAPI(GraphicsApi);
  virtual void destroyOutput() = 0;
  virtual std::shared_ptr<RenderState> renderState() const = 0;

  struct Configuration
  {
    // If set, the host is responsible for calling render() at this
    // rate (given in milliseconds)
    std::optional<double> manualRenderingRate;
    bool outputNeedsRenderPass{};
    OutputNode* parent{};
  };

  virtual Configuration configuration() const noexcept = 0;

protected:
  explicit OutputNode();
};
}
