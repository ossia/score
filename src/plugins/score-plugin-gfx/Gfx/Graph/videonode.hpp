#pragma once

#include "node.hpp"
#include "renderer.hpp"
#include "renderstate.hpp"
#include "uniforms.hpp"

#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Video/VideoInterface.hpp>

#include <ossia/detail/flicks.hpp>

#include <QElapsedTimer>
struct SCORE_PLUGIN_GFX_EXPORT VideoNode : NodeModel
{
  std::shared_ptr<video_decoder> decoder;
  std::atomic_bool seeked{};
  std::optional<double> nativeTempo;
  QString filter;

  const TexturedTriangle& m_mesh = TexturedTriangle::instance();
  VideoNode(
      std::shared_ptr<video_decoder> dec,
      std::optional<double> nativeTempo,
      QString f = {});

  const Mesh& mesh() const noexcept override;

  struct Rendered;

  virtual ~VideoNode();

  score::gfx::NodeRenderer* createRenderer(Renderer& r) const noexcept override;
};
