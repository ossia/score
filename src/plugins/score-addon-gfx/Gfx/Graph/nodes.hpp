#pragma once
#include "node.hpp"
#include "renderstate.hpp"
#include "uniforms.hpp"

class Window;
struct OutputNode : NodeModel
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

  virtual ~OutputNode() { }

  const TexturedTriangle& m_mesh = TexturedTriangle::instance();

  virtual void setRenderer(Renderer*) = 0;
  virtual Renderer* renderer() const = 0;

  virtual void startRendering() = 0;
  virtual void stopRendering() = 0;
  virtual bool canRender() const = 0;
  virtual void onRendererChange() = 0;

  virtual void createOutput(
      GraphicsApi graphicsApi,
      std::function<void()> onReady,
      std::function<void()> onUpdate,
      std::function<void()> onResize
      ) = 0;

  virtual void destroyOutput() = 0;
  virtual RenderState* renderState() const = 0;

protected:
  OutputNode() { setShaders(m_mesh.defaultVertexShader(), filter); }
  const Mesh& mesh() const noexcept override { return this->m_mesh; }
};

struct ColorNode : NodeModel
{
  static const constexpr auto filter = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    layout(std140, binding = 1) uniform buf {
        vec4 color;
    } ubuf;


    void main()
    {
        fragColor = ubuf.color;
    }
    )_";

  const TexturedTriangle& m_mesh = TexturedTriangle::instance();
  ColorNode()
  {
    setShaders(m_mesh.defaultVertexShader(), filter);
    input.push_back(new Port{this, {}, Types::Vec4, {}});
    // input.back()->value = ossia::vec4f{0.6, 0.3, 0.78, 1.};
    output.push_back(new Port{this, {}, Types::Image, {}});
  }

  const Mesh& mesh() const noexcept override { return this->m_mesh; }
  virtual ~ColorNode();
};

struct NoiseNode : NodeModel
{
  static const constexpr auto filter = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    void main()
    {
        fragColor = vec4(sin(v_texcoord.x * 100), 0., cos(v_texcoord.y * 100), 1.);
    }
    )_";

  const TexturedTriangle& m_mesh = TexturedTriangle::instance();
  NoiseNode()
  {
    setShaders(m_mesh.defaultVertexShader(), filter);
    output.push_back(new Port{this, {}, Types::Image, {}});
  }
  const Mesh& mesh() const noexcept override { return this->m_mesh; }
  virtual ~NoiseNode();
};

struct ProductNode : NodeModel
{
  static const constexpr auto filter = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    layout(binding = 1) uniform sampler2D t1;
    layout(binding = 2) uniform sampler2D t2;

    void main()
    {
        vec4 c1 = texture(t1, v_texcoord);
        vec4 c2 = texture(t2, v_texcoord);
        fragColor = c1 + c2;
    }
    )_";

  const TexturedTriangle& m_mesh = TexturedTriangle::instance();
  ProductNode()
  {
    setShaders(m_mesh.defaultVertexShader(), filter);
    input.push_back(new Port{this, {}, Types::Image, {}});
    input.push_back(new Port{this, {}, Types::Image, {}});
    output.push_back(new Port{this, {}, Types::Image, {}});
  }
  const Mesh& mesh() const noexcept override { return this->m_mesh; }
  virtual ~ProductNode();
};

struct ScreenNode : OutputNode
{
  ScreenNode(bool embedded = false);
  virtual ~ScreenNode();

  std::shared_ptr<Window> window{};
  QRhiSwapChain* swapChain{};

  void startRendering() override;
  void onRendererChange() override;
  bool canRender() const override;
  void stopRendering() override;

  void setRenderer(Renderer* r) override;
  Renderer* renderer() const override;

  void createOutput(
      GraphicsApi graphicsApi,
      std::function<void()> onReady,
      std::function<void()> onUpdate,
      std::function<void()> onResize
      ) override;
  void destroyOutput() override;

  RenderState* renderState() const override;
  RenderedNode* createRenderer() const noexcept override;

private:
  bool m_embedded{};
};

