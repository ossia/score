#include "GeometryToBuffer.hpp"

#include <QDebug>

#include <Threedim/Debug.hpp>
#include <halp/geometry.hpp>

namespace Threedim
{

GeometryToBuffer::GeometryToBuffer() { }

halp::attribute_location GeometryToBuffer::toAttributeLocation(Attribute attr) noexcept
{
  using namespace halp;
  switch(attr)
  {
    case Position:
      return attribute_location::position;
    case TexCoord:
      return attribute_location::tex_coord;
    case Color:
      return attribute_location::color;
    case Normal:
      return attribute_location::normal;
    case Tangent:
      return attribute_location::tangent;
  }
  return attribute_location::position;
}

void GeometryToBuffer::init(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  qDebug(Q_FUNC_INFO);
  const auto& mesh = inputs.geometry.mesh;

  if(mesh.vertices == 0)
  {
    qDebug() << "GeometryToBuffer::init - Empty mesh";
    return;
  }

  QRhi& rhi = *renderer.state.rhi;

  m_currentAttribute = inputs.attribute.value;
  m_currentPadToVec4 = inputs.pad_to_vec4.value;

  if(inputs.attribute.value < Attribute::Index)
  {
    // Normal attribute-semantic based strategy
    const auto location = toAttributeLocation(inputs.attribute.value);
    const auto lookup
        = inputs.attribute.value < Attribute::Attribute_0
              ? findAttribute(mesh, location)
              : findAttribute(mesh, inputs.attribute.value - Attribute::Attribute_0);

    if(!lookup)
    {
      qDebug() << "GeometryToBuffer::init - Attribute not found:"
               << magic_enum::enum_name(location);
      return;
    }

    const bool hasIndexBuffer = mesh.index.buffer >= 0;
    const bool canDirectRef = lookup->canDirectReference() && !hasIndexBuffer;

    bool success = false;

    if(hasIndexBuffer)
    {
      auto& strategy = m_strategy.emplace<IndexedExtractionStrategy>();
      success = strategy.init(renderer.state, rhi, mesh, *lookup, m_currentPadToVec4);
      if(!success)
      {
        qDebug() << "GeometryToBuffer::init - IndexedExtractionStrategy failed";
        m_strategy = std::monostate{};
      }
    }
    else if(canDirectRef)
    {
      auto& strategy = m_strategy.emplace<DirectReferenceStrategy>();
      success = strategy.init(renderer.state, rhi, mesh, *lookup, m_currentPadToVec4);
      if(!success)
      {
        qDebug() << "GeometryToBuffer::init - DirectReferenceStrategy failed";
        m_strategy = std::monostate{};
      }
    }
    else
    {
      auto& strategy = m_strategy.emplace<ComputeExtractionStrategy>();
      success = strategy.init(renderer.state, rhi, mesh, *lookup, m_currentPadToVec4);
      if(!success)
      {
        qDebug() << "GeometryToBuffer::init - ComputeExtractionStrategy failed";
        m_strategy = std::monostate{};
      }
    }

    updateOutput();
  }
  else if(inputs.attribute.value == Attribute::Index)
  {
    // Extract the input buffer if any
    if(mesh.index.buffer >= 0 && mesh.index.buffer < mesh.buffers.size())
    {
      auto& strategy = m_strategy.emplace<DirectBufferReferenceStrategy>();
      int64_t index_byte_size = 0;
      switch(mesh.index.format)
      {
        case halp::index_format::uint16:
          index_byte_size = mesh.vertices * 2;
          break;
        case halp::index_format::uint32:
          index_byte_size = mesh.vertices * 4;
          break;
      }

      const bool success = strategy.init(
          renderer.state, rhi, mesh, mesh.index.buffer, mesh.index.byte_offset,
          index_byte_size);
      if(!success)
      {
        qDebug() << "GeometryToBuffer::init - DirectBufferReferenceStrategy failed";
        m_strategy = std::monostate{};
      }
    }
  }
  else if(inputs.attribute.value <= Attribute::Buffer_8)
  {
    int buffer_index = inputs.attribute.value - std::to_underlying(Attribute::Buffer_0);
    if(buffer_index >= 0 && buffer_index < mesh.buffers.size())
    {
      // Extract a whole buffer by index
      auto& strategy = m_strategy.emplace<DirectBufferReferenceStrategy>();
      const bool success = strategy.init(
          renderer.state, rhi, mesh, buffer_index, 0,
          mesh.buffers[buffer_index].byte_size);
      if(!success)
      {
        qDebug() << "GeometryToBuffer::init - DirectBufferReferenceStrategy failed";
        m_strategy = std::monostate{};
      }
      else
      {
        qDebug() << "GeometryToBuffer::init - DirectBufferReferenceStrategy succeeded";
      }
    }
  }
  else
  {
    m_strategy = std::monostate{};
  }
}

void GeometryToBuffer::update(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res, score::gfx::Edge* e)
{
  const auto& mesh = inputs.geometry.mesh;

  if(mesh.vertices == 0)
    return;

  // Check if attribute or pad setting changed - requires full reinit
  const bool attributeChanged = (inputs.attribute.value != m_currentAttribute);
  const bool padChanged = (inputs.pad_to_vec4.value != m_currentPadToVec4);

  if(attributeChanged || padChanged)
  {
    release(renderer);
    init(renderer, res);
    return;
  }

  // FIXME:
  if(!inputs.geometry.dirty_mesh)
  {
    bool any_dirty = false;
    for(auto& buf : inputs.geometry.mesh.buffers)
    {
      qDebug() << "Dirty buffer yo";
      any_dirty |= buf.dirty;
    }
    if(!any_dirty)
      return;
  }

  QRhi& rhi = *renderer.state.rhi;

  if(inputs.attribute.value < Attribute::Index)
  {
    const auto location = toAttributeLocation(inputs.attribute.value);
    const auto lookup = findAttribute(mesh, location);

    if(!lookup)
    {
      qDebug() << "FAIL" << (int)location << bool(lookup);
      return;
    }

    // Check if strategy type needs to change
    const bool hasIndexBuffer = mesh.index.buffer >= 0;
    const bool canDirectRef = lookup->canDirectReference() && !hasIndexBuffer;

    const bool needsIndexed = hasIndexBuffer;
    const bool needsDirect = canDirectRef && !hasIndexBuffer;
    const bool needsCompute = !canDirectRef && !hasIndexBuffer;

    const bool isIndexed = std::holds_alternative<IndexedExtractionStrategy>(m_strategy);
    const bool isDirect = std::holds_alternative<DirectReferenceStrategy>(m_strategy);
    const bool isCompute = std::holds_alternative<ComputeExtractionStrategy>(m_strategy);

    // Strategy type changed - reinitialize
    if((needsIndexed && !isIndexed) || (needsDirect && !isDirect)
       || (needsCompute && !isCompute))
    {
      release(renderer);
      init(renderer, res);
      return;
    }

    // Update existing strategy
    std::visit([&](auto& strategy) {
      using T = std::decay_t<decltype(strategy)>;
      if constexpr(!std::is_same_v<T, std::monostate>)
      {
        strategy.update(rhi, mesh, *lookup, m_currentPadToVec4);
      }
    }, m_strategy);
  }
  else if(inputs.attribute.value == Attribute::Index)
  {
    const auto strategy = std::get_if<DirectBufferReferenceStrategy>(&m_strategy);
    if(!strategy)
    {
      release(renderer);
      init(renderer, res);
      return;
    }

    // Nothing to do in update
  }
  else if(inputs.attribute.value <= Attribute::Buffer_8)
  {
    const auto strategy = std::get_if<DirectBufferReferenceStrategy>(&m_strategy);
    if(!strategy)
    {
      release(renderer);
      init(renderer, res);
      return;
    }

    // Nothing to do in update
  }
  else
  {
    m_strategy = std::monostate{};
    return;
  }

  updateOutput();
}

void GeometryToBuffer::release(score::gfx::RenderList& renderer)
{
  std::visit([](auto& strategy) {
    using T = std::decay_t<decltype(strategy)>;
    if constexpr(!std::is_same_v<T, std::monostate>)
    {
      strategy.release();
    }
  }, m_strategy);

  m_strategy = std::monostate{};
  //  outputs.buffer.value = {};
}

void GeometryToBuffer::runInitialPasses(
    score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
    QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge)
{
  QRhi& rhi = *renderer.state.rhi;

  std::visit([&](auto& strategy) {
    using T = std::decay_t<decltype(strategy)>;
    if constexpr(!std::is_same_v<T, std::monostate>)
    {
      if constexpr(T::needsCompute())
      {
        strategy.runCompute(rhi, commands, res);
      }
    }
  }, m_strategy);
}

void GeometryToBuffer::updateOutput()
{
  std::visit([this](const auto& strategy) {
    using T = std::decay_t<decltype(strategy)>;
    if constexpr(!std::is_same_v<T, std::monostate>)
    {
      gpu_buffer_view out = strategy.output();
      outputs.buffer.buffer.handle = out.buffer;
      outputs.buffer.buffer.byte_size = out.size;
      outputs.buffer.buffer.byte_offset = out.offset;
    }
    else
    {
      outputs.buffer.buffer = {};
    }
  }, m_strategy);
}

void GeometryToBuffer::operator()()
{
  // qDebug() << inputs.geometry.mesh;
  return;
  /*
  outputs.buffer.buffer.byte_size = 0;
  if(inputs.geometry.mesh.buffers.size() > 0)
  {
    outputs.buffer.buffer.handle = inputs.geometry.mesh.buffers[0].handle;
    outputs.buffer.buffer.byte_size = inputs.geometry.mesh.buffers[0].byte_size;
  }*/
}
}
