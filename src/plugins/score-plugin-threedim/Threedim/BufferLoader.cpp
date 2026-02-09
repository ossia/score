#include "BufferLoader.hpp"

namespace Threedim
{
void SplatLoader::init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) {
}

void SplatLoader::update(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res, score::gfx::Edge* e)
{
  if(m_splat_data.splatCount <= 0 || !m_changed)
    return;

  const auto splats_byte_size = m_splat_data.buffer.size() * sizeof(float);
  qDebug() << splats_byte_size << (std::pow(2, 31) - 1);
  if(splats_byte_size >= std::numeric_limits<uint32_t>::max())
  {
    qDebug() << "Loaded model too large (Must be <4GB)";
    return;
  }
  if(!m_last_buffer)
  {
    m_last_buffer = renderer.state.rhi->newBuffer(
        QRhiBuffer::Immutable,
        QRhiBuffer::UsageFlag::VertexBuffer | QRhiBuffer::UsageFlag::StorageBuffer,
        splats_byte_size);
    if(!m_last_buffer->create())
    {
      release(renderer);
      return;
    }
  }
  else if(splats_byte_size != m_last_buffer->size())
  {
    m_last_buffer->destroy();
    m_last_buffer->setSize(splats_byte_size);
    if(!m_last_buffer->create())
    {
      release(renderer);
      return;
    }
  }

  res.uploadStaticBuffer(
      m_last_buffer,
      QByteArray::fromRawData((char*)m_splat_data.buffer.data(), splats_byte_size));

  outputs.buffer.buffer.handle = m_last_buffer;
  outputs.buffer.buffer.byte_size = m_last_buffer->size();
  outputs.buffer.buffer.changed = true;
  m_changed = false;
}

void SplatLoader::release(score::gfx::RenderList& r)
{
  r.releaseBuffer(m_last_buffer);
  m_last_buffer = nullptr;
  outputs.buffer.buffer.handle = nullptr;
  outputs.buffer.buffer.byte_size = 0;
  outputs.buffer.buffer.changed = true;
}

void SplatLoader::runInitialPasses(
    score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
    QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge)
{
}

void SplatLoader::operator()() { }
}
