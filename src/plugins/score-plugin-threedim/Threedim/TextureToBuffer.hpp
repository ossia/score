#pragma once

#include <ossia/detail/pod_vector.hpp>
#include <Threedim/GeometryToBufferStrategies.hpp>

#include <boost/container/vector.hpp>

#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>
#include <halp/buffer.hpp>
#include <halp/texture.hpp>

namespace Threedim
{
class TextureToBuffer
{
public:
  halp_meta(name, "Texture to buffer")
  halp_meta(category, "Visuals/Utilities")
  halp_meta(c_name, "texture_to_buffer")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/texture-to-buffer.html")
  halp_meta(uuid, "fd7d6339-c745-4733-a1c2-6ebd0a25fd92")

  struct ins
  {
    halp::texture_input<"Texture", halp::custom_variable_texture> texture;
  } inputs;
  struct
  {
    halp::gpu_buffer_output<"Buffer"> buffer;
  } outputs;

  QRhiBuffer* buf{};
  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res)
  {
    outputs.buffer.buffer.handle = nullptr;
    outputs.buffer.buffer.byte_size = 0;
    outputs.buffer.buffer.byte_offset = 0;
    outputs.buffer.buffer.changed = true;

    if(!inputs.texture.texture.bytes)
      return;
    auto bytes = inputs.texture.texture.bytesize();

    if(buf)
      renderer.releaseBuffer(buf);
    if(bytes <= 0)
      return;

    buf = renderer.state.rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, bytes);
    if(!buf->create())
    {
      delete buf;
      buf = nullptr;
      return;
    }

    outputs.buffer.buffer.handle = buf;
    outputs.buffer.buffer.byte_size = bytes;
    outputs.buffer.buffer.byte_offset = 0;
    outputs.buffer.buffer.changed = true;
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e)
  {
    if(!buf || inputs.texture.texture.bytesize() != buf->size())
      init(renderer, res);
  }

  void release(score::gfx::RenderList& r)
  {
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge)
  {
    if(!inputs.texture.texture.bytes)
      return;
    if(!buf)
      return;
    auto sz = buf->size();
    if(inputs.texture.texture.bytesize() != sz)
      return;


    res->uploadStaticBuffer(
        buf,
        QByteArray::fromRawData((const char*)inputs.texture.texture.bytes, sz));
  }
};

}
