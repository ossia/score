#pragma once

#include "node.hpp"
#include "renderer.hpp"
#include "renderstate.hpp"
#include "uniforms.hpp"
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

#include <QElapsedTimer>
#include <Video/VideoInterface.hpp>
#include <ossia/detail/flicks.hpp>
struct VideoNode : NodeModel
{
  std::shared_ptr<video_decoder> decoder;
  std::unique_ptr<GPUVideoDecoder> gpu;
  AVPixelFormat current_format = AV_PIX_FMT_YUV420P;
  std::atomic_bool seeked{};
  std::optional<double> nativeTempo;
  QString filter;

  const TexturedTriangle& m_mesh = TexturedTriangle::instance();
  VideoNode(std::shared_ptr<video_decoder> dec
            , std::optional<double> nativeTempo
            , QString f = {});

  void initGpuDecoder();
  void checkFormat(AVPixelFormat fmt);

  const Mesh& mesh() const noexcept override;

  struct Rendered;

  virtual ~VideoNode();

  score::gfx::NodeRenderer* createRenderer() const noexcept;
};
