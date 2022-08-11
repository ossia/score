#pragma once

#include <score/graphics/GraphicsLayout.hpp>

namespace score
{

class SCORE_LIB_BASE_EXPORT GraphicsHBoxLayout : public GraphicsLayout
{
public:
  using GraphicsLayout::GraphicsLayout;
  ~GraphicsHBoxLayout();

  void layout() override;
  void centerContent() override;
};
class SCORE_LIB_BASE_EXPORT GraphicsVBoxLayout : public GraphicsLayout
{
public:
  using GraphicsLayout::GraphicsLayout;
  ~GraphicsVBoxLayout();
  void layout() override;
  void centerContent() override;
};
}
