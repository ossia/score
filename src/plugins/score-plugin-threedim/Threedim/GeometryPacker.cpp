#include "GeometryPacker.hpp"

#include <Gfx/Graph/RenderList.hpp>

#include <Threedim/Debug.hpp>

namespace Threedim
{

void GeometryPacker::buildAttributeSpecs()
{
  using namespace halp;
  m_specs.clear();

  // Order matters - this defines the interleaving order
  switch(inputs.position.value)
  {
    case Attribute3::Vec3:
      m_specs.push_back(
          {.location = attribute_location::position, .pad_to_vec4 = false});
      break;
    case Attribute3::Vec4:
      m_specs.push_back({.location = attribute_location::position, .pad_to_vec4 = true});
      break;
    default:
      break;
  }

  switch(inputs.normal.value)
  {
    case Attribute3::Vec3:
      m_specs.push_back({.location = attribute_location::normal, .pad_to_vec4 = false});
      break;
    case Attribute3::Vec4:
      m_specs.push_back({.location = attribute_location::normal, .pad_to_vec4 = true});
      break;
    default:
      break;
  }

  switch(inputs.color.value)
  {
    case Attribute3::Vec3:
      m_specs.push_back({.location = attribute_location::color, .pad_to_vec4 = false});
      break;
    case Attribute3::Vec4:
      m_specs.push_back({.location = attribute_location::color, .pad_to_vec4 = true});
      break;
    default:
      break;
  }

  switch(inputs.texcoord.value)
  {
    case Attribute2::Vec2:
      m_specs.push_back(
          {.location = attribute_location::tex_coord, .pad_to_vec4 = false});
      break;
    default:
      break;
  }

  switch(inputs.tangent.value)
  {
    case Attribute3::Vec3:
      m_specs.push_back({.location = attribute_location::tangent, .pad_to_vec4 = false});
      break;
    case Attribute3::Vec4:
      m_specs.push_back({.location = attribute_location::tangent, .pad_to_vec4 = true});
      break;
    default:
      break;
  }
}

bool GeometryPacker::specsChanged() const
{
  if(m_specs.size() != m_lastSpecs.size())
    return true;

  for(size_t i = 0; i < m_specs.size(); ++i)
  {
    if(m_specs[i].location != m_lastSpecs[i].location
       || m_specs[i].pad_to_vec4 != m_lastSpecs[i].pad_to_vec4)
    {
      return true;
    }
  }
  return false;
}

void GeometryPacker::init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  const auto& mesh = inputs.geometry.mesh;

  if(mesh.vertices == 0)
  {
    qDebug() << "GeometryPacker::init - Empty mesh";
    return;
  }

  buildAttributeSpecs();

  if(m_specs.empty())
  {
    qDebug() << "GeometryPacker::init - No attributes enabled";
    return;
  }

  QRhi& rhi = *renderer.state.rhi;

  if(!m_strategy.init(renderer.state, rhi, mesh, m_specs))
  {
    qDebug() << "GeometryPacker::init - Strategy init failed";
    return;
  }

  m_lastSpecs = m_specs;
  m_initialized = true;

  const auto out = m_strategy.output();
  outputs.buffer.buffer.handle = out.buffer;
  outputs.buffer.buffer.byte_offset = out.offset;
  outputs.buffer.buffer.byte_size = out.size;
  outputs.stride.value = m_strategy.outputStride();
}

void GeometryPacker::update(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res, score::gfx::Edge* e)
{
  const auto& mesh = inputs.geometry.mesh;

  if(mesh.vertices == 0)
    return;

  buildAttributeSpecs();

  // Check if configuration changed - requires full reinit
  if(specsChanged())
  {
    release(renderer);
    init(renderer, res);
    return;
  }

  if(!m_initialized)
    return;
  // FIXME
  // if(!inputs.geometry.dirty_mesh)
  //   return;

  QRhi& rhi = *renderer.state.rhi;

  m_strategy.update(rhi, mesh, m_specs);

  const auto out = m_strategy.output();
  outputs.buffer.buffer.handle = out.buffer;
  outputs.buffer.buffer.byte_offset = out.offset;
  outputs.buffer.buffer.byte_size = out.size;
  outputs.stride.value = m_strategy.outputStride();
}

void GeometryPacker::release(score::gfx::RenderList& renderer)
{
  m_strategy.release();
  m_initialized = false;
  m_lastSpecs.clear();
  outputs.buffer = {};
  outputs.stride.value = 0;
}

void GeometryPacker::runInitialPasses(
    score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
    QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge)
{
  if(!m_initialized)
    return;

  QRhi& rhi = *renderer.state.rhi;
  m_strategy.runCompute(rhi, commands, res);
}

} // namespace halp
