#pragma once

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Video/VideoInterface.hpp>

extern "C"
{
#include <libavutil/pixdesc.h>
}
namespace score::gfx
{
// TODO the "model" nodes should have a first update step so that they
// can share data across all renderers during a tick
class VideoNode;

/**
 * @brief Processes and renders a video frame on the GPU
 *
 * This class is used as a base type for GPU decoders.
 *
 * Child classes must :
 *
 * - Create relevant shaders, samplers & textures in the init method.
 * - When exec is called, copy the data from the AVFrame to the QRhiTextures.
 *
 * See RGB0Decoder for an example with a single texture, YUV420Decoder for an
 * example with multiple textures.
 */
class GPUVideoDecoder
{
public:
  GPUVideoDecoder();
  virtual ~GPUVideoDecoder();

  /**
   * @brief Initialize a GPUVideoDecoder
   *
   * This method must :
   * - Create samplers and textures for the video format.
   * - Create shaders that will render the data put into these textures.
   *
   * It returns a {vertex, fragment} shader pair.
   */
  [[nodiscard]]
  virtual std::pair<QShader, QShader> init(RenderList& r) = 0;

  /**
   * @brief Decode and upload a video frame to the GPU.
   */
  virtual void exec(
      RenderList&,
      QRhiResourceUpdateBatch& res,
      AVFrame& frame)
      = 0;

  /**
   * @brief This method will release all the created samplers and textures.
   */
  void release(RenderList&);

  /**
   * @brief Utility method to create a QRhiTextureSubresourceUploadDescription.
   *
   * If possible, it tries to avoid a copy of pixels : pixels must not be freed before the
   * frame has been rendered.
   */
  static QRhiTextureSubresourceUploadDescription createTextureUpload(
      uint8_t* pixels,
      int w,
      int h,
      int bytesPerPixel,
      int stride);

  static QString vertexShader() noexcept;

  std::vector<Sampler> samplers;
};

/**
 * @brief Default decoder when we do not know what to render.
 */
struct EmptyDecoder : GPUVideoDecoder
{
  static const constexpr auto hashtag_no_filter = R"_(#version 450
    void main ()
    {
    }
  )_";

  explicit EmptyDecoder()
  {
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    return score::gfx::makeShaders(vertexShader(), hashtag_no_filter);
  }

  void exec(
      RenderList&,
      QRhiResourceUpdateBatch& res,
      AVFrame& frame) override
  {
  }
};
}
