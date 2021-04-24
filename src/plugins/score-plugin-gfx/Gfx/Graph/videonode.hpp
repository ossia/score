#pragma once

#include "node.hpp"
#include "renderer.hpp"
#include "renderstate.hpp"
#include "uniforms.hpp"
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

#include <QElapsedTimer>
#include <Video/VideoInterface.hpp>
#include <ossia/detail/flicks.hpp>
struct SCORE_PLUGIN_GFX_EXPORT VideoNode : NodeModel
{
  std::shared_ptr<video_decoder> decoder;
  std::unique_ptr<GPUVideoDecoder> gpu;
  AVPixelFormat current_format = AVPixelFormat(-1);
  int current_width{}, current_height{};
  std::atomic_bool seeked{};
  std::optional<double> nativeTempo;
  QString filter;

  const TexturedTriangle& m_mesh = TexturedTriangle::instance();
  VideoNode(std::shared_ptr<video_decoder> dec
            , std::optional<double> nativeTempo
            , QString f = {});

  void initGpuDecoder();
  void checkFormat(AVPixelFormat fmt, int w, int h);

  const Mesh& mesh() const noexcept override;

  struct Rendered;

  virtual ~VideoNode();

  score::gfx::NodeRenderer* createRenderer() const noexcept override;
};
