#pragma once
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Uniforms.hpp>

#include <score_plugin_gfx_export.h>
namespace score::gfx
{

class Window;
/**
 * @brief Base class for sink nodes (QWindow, spout, syphon, NDI output, ...)
 */
struct SCORE_PLUGIN_GFX_EXPORT OutputNode : NodeModel
{
  static const constexpr auto filter = R"_(#version 450
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 3) uniform sampler2D tex;

void main()
{
    fragColor = texture(tex, v_texcoord);
}
)_";

  virtual ~OutputNode();

  const TexturedTriangle& m_mesh = TexturedTriangle::instance();

  virtual void setRenderer(RenderList*) = 0;
  virtual RenderList* renderer() const = 0;

  virtual void startRendering() = 0;
  virtual void stopRendering() = 0;
  virtual bool canRender() const = 0;
  virtual void onRendererChange() = 0;

  virtual void createOutput(
      GraphicsApi graphicsApi,
      std::function<void()> onReady,
      std::function<void()> onUpdate,
      std::function<void()> onResize)
      = 0;

  virtual void updateGraphicsAPI(GraphicsApi);
  virtual void destroyOutput() = 0;
  virtual RenderState* renderState() const = 0;

protected:
  explicit OutputNode();
  const Mesh& mesh() const noexcept override { return this->m_mesh; }
};
}
