#pragma once

#include <Gfx/Graph/Node.hpp>

#include <QFont>
#include <QPen>

namespace score::gfx
{
/**
 * @brief A node that renders text to screen.
 */
struct TextNode : NodeModel
{
public:
  explicit TextNode();
  virtual ~TextNode();

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  void process(Message&& msg) override;
  class Renderer;

#pragma pack(push, 1)
  struct UBO
  {
    float opacity{1.};
    float padding_std140_0{};
    // Clip-space offset (added to the text quad in the vertex shader). {0,0} is
    // screen centre; the previous {0.5,0.5} default shifted an unconfigured Text
    // process ~half a screen up+right — off the top edge — so default text was
    // invisible until Position was touched. (video/text render validation)
    float position[2]{0.0, 0.0};
    float scale[2]{1., 1.};
  } ubo;
#pragma pack(pop)

  QString text;
  QFont font;
  QPen pen;

  std::atomic_int64_t mustRerender{0};

private:
};

}
