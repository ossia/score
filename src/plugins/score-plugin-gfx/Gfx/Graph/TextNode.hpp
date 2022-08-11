#pragma once

#include <Gfx/Graph/Node.hpp>

#include <ossia/detail/packed_struct.hpp>

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
  struct UBO
  {
    float opacity{1.};
    float padding_std140_0{};
    float position[2]{0.5, 0.5};
    float scale[2]{1., 1.};
  } ubo;

  QString text;
  QFont font;
  QPen pen;

  std::atomic_bool mustRerender{true};

private:
};

}
