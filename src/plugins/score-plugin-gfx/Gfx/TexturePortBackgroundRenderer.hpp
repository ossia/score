#pragma once
#include <Gfx/TexturePort.hpp>

#include <score/graphics/BackgroundRenderer.hpp>

namespace Gfx
{
class TextureOutletBackgroundRendererFactory : public score::BackgroundRendererFactory
{
  SCORE_CONCRETE("ce4db759-3e9c-4ad4-bc09-16f77069129e")
public:
  ~TextureOutletBackgroundRendererFactory() override;

  bool matches(const Selection& sel, QObject* parent) const noexcept override;

  score::BackgroundRenderer* make(const Selection& sel, QObject* parent) const override;
};
}
