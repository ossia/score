#include "ExtractBuffer2.hpp"

#include <Threedim/Debug.hpp>

#include <QDebug>

#include <charconv>
#include <string_view>

namespace Threedim
{
namespace
{
// Tiny helper: parse `n` as a non-negative integer. Returns -1 on miss.
[[nodiscard]] int parseInt(std::string_view n) noexcept
{
  int v{};
  const auto* first = n.data();
  const auto* last = n.data() + n.size();
  auto [ptr, ec] = std::from_chars(first, last, v);
  if(ec != std::errc{} || ptr != last || v < 0)
    return -1;
  return v;
}

// Map a user-supplied name to a halp::attribute_semantic. Returns
// nullopt for unknown names (the caller then falls back to the
// custom-name lookup against geometry_attribute::name).
[[nodiscard]] std::optional<halp::attribute_semantic>
nameToSemantic(std::string_view n) noexcept
{
  using S = halp::attribute_semantic;
  // FIXME add all the others
  if(n == "position" || n == "pos")
    return S::position;
  if(n == "normal" || n == "norm")
    return S::normal;
  if(n == "tangent")
    return S::tangent;
  if(n == "bitangent")
    return S::bitangent;
  if(n == "uv" || n == "texcoord" || n == "texcoord0")
    return S::texcoord0;
  if(n == "texcoord1")
    return S::texcoord1;
  if(n == "texcoord2")
    return S::texcoord2;
  if(n == "texcoord3")
    return S::texcoord3;
  if(n == "color" || n == "color0")
    return S::color0;
  if(n == "color1")
    return S::color1;
  if(n == "color2")
    return S::color2;
  if(n == "color3")
    return S::color3;
  if(n == "joints" || n == "joints0")
    return S::joints0;
  if(n == "joints1")
    return S::joints1;
  if(n == "weights" || n == "weights0")
    return S::weights0;
  if(n == "weights1")
    return S::weights1;
  if(n == "velocity")
    return S::velocity;
  return std::nullopt;
}
}

ExtractBuffer2::ExtractBuffer2() = default;

std::optional<attribute_lookup> ExtractBuffer2::resolveAttribute(
    const halp::dynamic_gpu_geometry& mesh, std::string_view n) noexcept
{
  if(n.empty())
    return std::nullopt;

  // Numeric -> Nth attribute slot.
  if(const int idx = parseInt(n); idx >= 0)
    return findAttribute(mesh, idx);

  // Well-known semantic name.
  if(const auto sem = nameToSemantic(n))
    return findAttribute(mesh, *sem);

  // Custom-name lookup against geometry_attribute::name.
  for(int i = 0; i < (int)mesh.attributes.size(); ++i)
  {
    if(mesh.attributes[i].name == n)
      return findAttribute(mesh, i);
  }

  return std::nullopt;
}

ExtractBuffer2::BufferRef ExtractBuffer2::resolveBuffer(
    const halp::dynamic_gpu_geometry& mesh, std::string_view n) noexcept
{
  if(n.empty())
    return {};

  // "index" -> the index buffer
  if(n == "index")
  {
    if(mesh.index.buffer < 0 || mesh.index.buffer >= (int)mesh.buffers.size())
      return {};
    // The index buffer length is mesh.indices (the index-element count), which
    // is a distinct field from mesh.vertices. A zero count means a
    // non-indexed / unpopulated mesh: clear the outlet rather than publishing a
    // garbage-sized range.
    if(mesh.indices <= 0)
      return {};
    int64_t bytes = 0;
    switch(mesh.index.format)
    {
      case halp::index_format::uint16:
        bytes = (int64_t)mesh.indices * 2;
        break;
      case halp::index_format::uint32:
        bytes = (int64_t)mesh.indices * 4;
        break;
    }
    return {
        .buffer_index = mesh.index.buffer,
        .byte_offset = mesh.index.byte_offset,
        .byte_size = bytes};
  }

  // Numeric -> Nth buffer in mesh.buffers[]
  if(const int idx = parseInt(n); idx >= 0)
  {
    if(idx >= (int)mesh.buffers.size())
      return {};
    return {
        .buffer_index = idx,
        .byte_offset = 0,
        .byte_size = mesh.buffers[idx].byte_size};
  }

  // Named auxiliary buffer (scene_lights, scene_materials, model_matrices, ...).
  // ScenePreprocessor and other producers attach scene-level data here. Checked
  // first because aux names are user-chosen and may shadow attribute names.
  for(const auto& aux : mesh.auxiliary)
  {
    if(aux.name == n)
    {
      if(aux.buffer < 0 || aux.buffer >= (int)mesh.buffers.size())
        return {};
      const int64_t size
          = aux.byte_size > 0 ? aux.byte_size : mesh.buffers[aux.buffer].byte_size;
      return {
          .buffer_index = aux.buffer,
          .byte_offset = aux.byte_offset,
          .byte_size = size};
    }
  }

  // Otherwise: try to resolve as an attribute name and walk to the
  // backing buffer.
  if(const auto lk = resolveAttribute(mesh, n); lk && lk->input)
  {
    const int bidx = lk->input->buffer;
    if(bidx >= 0 && bidx < (int)mesh.buffers.size())
    {
      return {
          .buffer_index = bidx,
          .byte_offset = 0,
          .byte_size = mesh.buffers[bidx].byte_size};
    }
  }

  return {};
}

void ExtractBuffer2::initStrategy(score::gfx::RenderList& renderer)
{
  const auto& mesh = inputs.geometry.mesh;
  if(mesh.vertices == 0)
  {
    m_strategy = std::monostate{};
    return;
  }

  QRhi& rhi = *renderer.state.rhi;

  m_currentMode = inputs.mode.value;
  m_currentName = inputs.name.value;
  m_currentPadToVec4 = inputs.pad_to_vec4.value;

  if(inputs.mode.value == Attribute)
  {
    const auto lookup = resolveAttribute(mesh, m_currentName);
    if(!lookup)
    {
      qWarning() << this << "ExtractBuffer2: attribute not found:"
               << QString::fromStdString(m_currentName);
      m_strategy = std::monostate{};
      return;
    }
    if(!lookup->buffer || !lookup->buffer->handle)
    {
      qWarning() << this << "ExtractBuffer2: source buffer is null";
      m_strategy = std::monostate{};
      return;
    }

    const bool hasIndexBuffer = mesh.index.buffer >= 0;
    const bool canDirectRef = lookup->canDirectReference() && !hasIndexBuffer;

    bool ok = false;
    if(hasIndexBuffer)
    {
      auto& s = m_strategy.emplace<IndexedExtractionStrategy>();
      ok = s.init(renderer.state, rhi, mesh, *lookup, m_currentPadToVec4);
    }
    else if(canDirectRef)
    {
      auto& s = m_strategy.emplace<DirectReferenceStrategy>();
      ok = s.init(renderer.state, rhi, mesh, *lookup, m_currentPadToVec4);
    }
    else
    {
      auto& s = m_strategy.emplace<ComputeExtractionStrategy>();
      ok = s.init(renderer.state, rhi, mesh, *lookup, m_currentPadToVec4);
    }
    if(!ok)
    {
      qWarning() << this << "ExtractBuffer2: strategy init failed";
      m_strategy = std::monostate{};
    }
  }
  else // Buffer
  {
    const auto ref = resolveBuffer(mesh, m_currentName);
    if(ref.buffer_index < 0 || ref.byte_size <= 0)
    {
      qWarning() << this << "ExtractBuffer2: buffer not found:"
               << QString::fromStdString(m_currentName);
      m_strategy = std::monostate{};
      return;
    }
    auto& s = m_strategy.emplace<DirectBufferReferenceStrategy>();
    if(!s.init(renderer.state, rhi, mesh, ref.buffer_index, ref.byte_offset, ref.byte_size))
    {
      qWarning() << this << "ExtractBuffer2: DirectBufferReferenceStrategy failed";
      m_strategy = std::monostate{};
    }
  }
}

void ExtractBuffer2::init(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  initStrategy(renderer);
  updateOutput();
}

void ExtractBuffer2::update(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
    score::gfx::Edge* /*e*/)
{
  const auto& mesh = inputs.geometry.mesh;
  if(mesh.vertices == 0)
    return;

  // Selector or pad change -> tear down and rebuild from scratch. The
  // strategies are cheap to recreate (they own at most one compute
  // pipeline) so this keeps the update path simple.
  const bool modeChanged = (inputs.mode.value != m_currentMode);
  const bool nameChanged = (inputs.name.value != m_currentName);
  const bool padChanged = (inputs.pad_to_vec4.value != m_currentPadToVec4);
  if(modeChanged || nameChanged || padChanged)
  {
    release(renderer);
    initStrategy(renderer);
    updateOutput();
    return;
  }

  // Drain dirty flags so the upstream knows we picked them up. We
  // always re-check the source buffer pointers below regardless.
  bool any_dirty = inputs.geometry.dirty_mesh;
  for(auto& buf : inputs.geometry.mesh.buffers)
  {
    any_dirty |= buf.dirty;
    buf.dirty = false;
  }
  inputs.geometry.dirty_mesh = false;

  if(inputs.mode.value == Attribute)
  {
    const auto lookup = resolveAttribute(mesh, m_currentName);
    if(!lookup)
      return;

    // Strategy class may need to change if the upstream changed its
    // index/binding layout (e.g. went from non-indexed to indexed).
    const bool hasIndexBuffer = mesh.index.buffer >= 0;
    const bool canDirectRef = lookup->canDirectReference() && !hasIndexBuffer;

    const bool needsIndexed = hasIndexBuffer;
    const bool needsDirect = canDirectRef && !hasIndexBuffer;
    const bool needsCompute = !canDirectRef && !hasIndexBuffer;
    const bool isIndexed = std::holds_alternative<IndexedExtractionStrategy>(m_strategy);
    const bool isDirect = std::holds_alternative<DirectReferenceStrategy>(m_strategy);
    const bool isCompute = std::holds_alternative<ComputeExtractionStrategy>(m_strategy);

    if((needsIndexed && !isIndexed) || (needsDirect && !isDirect)
       || (needsCompute && !isCompute))
    {
      release(renderer);
      initStrategy(renderer);
      updateOutput();
      return;
    }

    QRhi& rhi = *renderer.state.rhi;
    std::visit(
        [&](auto& strategy) {
      using T = std::decay_t<decltype(strategy)>;
      if constexpr(!std::is_same_v<T, std::monostate>)
        strategy.update(rhi, mesh, *lookup, m_currentPadToVec4);
        },
        m_strategy);
  }
  else // Buffer
  {
    auto* strat = std::get_if<DirectBufferReferenceStrategy>(&m_strategy);
    if(!strat)
    {
      release(renderer);
      initStrategy(renderer);
      updateOutput();
      return;
    }

    // Re-resolve and re-init in place: even if the user-visible name
    // hasn't changed, the upstream may have rebuilt the QRhiBuffer*
    // (resize, format change). DirectBufferReferenceStrategy is
    // pointer-only state, so this is effectively just a re-fetch.
    const auto ref = resolveBuffer(mesh, m_currentName);
    if(ref.buffer_index < 0 || ref.byte_size <= 0)
    {
      release(renderer);
      return;
    }
    QRhi& rhi = *renderer.state.rhi;
    if(!strat->init(
           renderer.state, rhi, mesh, ref.buffer_index, ref.byte_offset,
           ref.byte_size))
    {
      qWarning() << this << "ExtractBuffer2: re-init failed in update";
      release(renderer);
      return;
    }
  }

  updateOutput();
}

void ExtractBuffer2::release(score::gfx::RenderList& /*renderer*/)
{
  std::visit(
      [](auto& strategy) {
    using T = std::decay_t<decltype(strategy)>;
    if constexpr(!std::is_same_v<T, std::monostate>)
      strategy.release();
      },
      m_strategy);
  m_strategy = std::monostate{};
}

void ExtractBuffer2::runInitialPasses(
    score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
    QRhiResourceUpdateBatch*& res, score::gfx::Edge& /*edge*/)
{
  QRhi& rhi = *renderer.state.rhi;
  std::visit(
      [&](auto& strategy) {
    using T = std::decay_t<decltype(strategy)>;
    if constexpr(!std::is_same_v<T, std::monostate>)
    {
      if constexpr(T::needsCompute())
        strategy.runCompute(rhi, commands, res);
    }
      },
      m_strategy);
}

void ExtractBuffer2::updateOutput()
{
  std::visit(
      [this](const auto& strategy) {
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
      },
      m_strategy);
}

void ExtractBuffer2::operator()() { }
}
