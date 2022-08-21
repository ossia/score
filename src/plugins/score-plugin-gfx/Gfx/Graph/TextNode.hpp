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
    float position[2]{0.5, 0.5};
    float scale[2]{1., 1.};
  } ubo;
#pragma pack(pop)

  QString text;
  QFont font;
  QPen pen;

  std::atomic_bool mustRerender{true};

private:
};

}
