#include "ExtractTexture.hpp"

#include <Gfx/Graph/RenderList.hpp>

#include <QtGui/private/qrhi_p.h>

namespace Threedim
{

void ExtractTexture::init(
    score::gfx::RenderList& /*renderer*/, QRhiResourceUpdateBatch& /*res*/)
{
}

void ExtractTexture::update(
    score::gfx::RenderList& /*renderer*/, QRhiResourceUpdateBatch& /*res*/,
    score::gfx::Edge* /*e*/)
{
  const auto& mesh = inputs.geometry.mesh;
  const auto& target_name = inputs.name.value;

  // Resolve by name. aux names are producer-chosen (ScenePreprocessor
  // uses "skybox", "irradiance_map", "camera", "base_color_array",
  // …); if the target is missing we hand out a null handle so the
  // downstream binding drops to its empty-placeholder.
  void* resolved = nullptr;
  void* resolved_sampler = nullptr;
  for(const auto& aux : mesh.auxiliary_textures)
  {
    if(aux.name == target_name)
    {
      resolved = aux.handle;
      resolved_sampler = aux.sampler_handle;
      break;
    }
  }

  // Short-circuit identical-state updates. Texture metadata re-emission
  // trips downstream SRB rebuilds, so we only publish when the handle
  // pointer or the target name actually changed.
  if(resolved == m_lastHandle && target_name == m_lastName)
    return;
  m_lastHandle = resolved;
  m_lastName = target_name;

  outputs.texture.texture.handle = resolved;
  // Forward the producer-side sampler if any. ScenePreprocessor's per-
  // bucket sampler split (per-glTF wrap/filter mode) ships a sampler
  // alongside each material texture array — passing it through here
  // lets downstream sampler-config-sensitive nodes (anisotropy, custom
  // wrap mode) honour it. Null = downstream falls back to its own.
  outputs.texture.texture.sampler_handle = resolved_sampler;

  if(!resolved)
  {
    outputs.texture.texture.width = 0;
    outputs.texture.texture.height = 0;
    outputs.texture.texture.layers_or_depth = 1;
    outputs.texture.texture.kind = halp::texture_kind::texture_2d;
    return;
  }

  // Detect the texture shape from the live QRhiTexture's flags +
  // dimensions. Order matters: CubeMap and ThreeDimensional are
  // mutually exclusive by construction, but check CubeMap first as
  // some backends may happen to set both bits on edge-case allocations.
  auto* tex = static_cast<QRhiTexture*>(resolved);
  const auto flags = tex->flags();
  const QSize px = tex->pixelSize();

  outputs.texture.texture.width = px.width();
  outputs.texture.texture.height = px.height();

  if(flags.testFlag(QRhiTexture::CubeMap))
  {
    outputs.texture.texture.kind = halp::texture_kind::cubemap;
    outputs.texture.texture.layers_or_depth = 6;
  }
  else if(flags.testFlag(QRhiTexture::ThreeDimensional))
  {
    outputs.texture.texture.kind = halp::texture_kind::texture_3d;
    // QRhiTexture::depth() is 0 for non-3D textures, set on allocation
    // for 3D. Default to 1 when the backend returns 0 on a 3D texture
    // that hasn't been filled yet — avoids an illegal 0-depth probe
    // binding downstream.
    outputs.texture.texture.layers_or_depth = std::max(1, tex->depth());
  }
  else if(flags.testFlag(QRhiTexture::TextureArray))
  {
    outputs.texture.texture.kind = halp::texture_kind::texture_array;
    outputs.texture.texture.layers_or_depth = std::max(1, tex->arraySize());
  }
  else
  {
    outputs.texture.texture.kind = halp::texture_kind::texture_2d;
    outputs.texture.texture.layers_or_depth = 1;
  }

  // Format reporting — halp's gpu_texture format taxonomy now mirrors
  // QRhi's color + integer set, so downstream nodes that branch on
  // format (HDR-ness, integer-vs-float for atomic-image consumers,
  // sRGB inference) get a faithful answer instead of the previous
  // "everything not in the float subset → RGBA8" silent miscast.
  //
  // QRhi version availability:
  //   - RGBA8 / BGRA8 / R8 / RG8 / R16 / RG16 / float family / depth →
  //     present since QRhi went public-ish (Qt 6.2 private API).
  //   - RGB10A2 added in Qt 6.4.
  //   - Integer family (R8UI / R32UI / RG32UI / RGBA32UI / *SI variants)
  //     added in Qt 6.10. Guard so older Qt builds compile.
  switch(tex->format())
  {
    // 8-bit unorm — Qt 6.2+
    case QRhiTexture::RGBA8:    outputs.texture.texture.format = halp::gpu_texture::RGBA8;    break;
    case QRhiTexture::BGRA8:    outputs.texture.texture.format = halp::gpu_texture::BGRA8;    break;
    case QRhiTexture::R8:       outputs.texture.texture.format = halp::gpu_texture::R8;       break;
    case QRhiTexture::RG8:      outputs.texture.texture.format = halp::gpu_texture::RG8;      break;

    // 16-bit unorm — Qt 6.2+
    case QRhiTexture::R16:      outputs.texture.texture.format = halp::gpu_texture::R16;      break;
    case QRhiTexture::RG16:     outputs.texture.texture.format = halp::gpu_texture::RG16;     break;

    // float — Qt 6.2+
    case QRhiTexture::RGBA16F:  outputs.texture.texture.format = halp::gpu_texture::RGBA16F;  break;
    case QRhiTexture::RGBA32F:  outputs.texture.texture.format = halp::gpu_texture::RGBA32F;  break;
    case QRhiTexture::R16F:     outputs.texture.texture.format = halp::gpu_texture::R16F;     break;
    case QRhiTexture::R32F:     outputs.texture.texture.format = halp::gpu_texture::R32F;     break;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    // 10/10/10/2 packed — Qt 6.4+
    case QRhiTexture::RGB10A2:  outputs.texture.texture.format = halp::gpu_texture::RGB10A2;  break;
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    // Unsigned integer — Qt 6.10+. REQUIRED to be reported as such for
    // atomic-image consumers (voxelizer occupancy grids, histogram
    // targets, …). A miscast here would tell downstream "this is RGBA8,
    // sample as float" and break uimage / usampler bindings on Vulkan
    // validation.
    case QRhiTexture::R8UI:     outputs.texture.texture.format = halp::gpu_texture::R8UI;     break;
    case QRhiTexture::R32UI:    outputs.texture.texture.format = halp::gpu_texture::R32UI;    break;
    case QRhiTexture::RG32UI:   outputs.texture.texture.format = halp::gpu_texture::RG32UI;   break;
    case QRhiTexture::RGBA32UI: outputs.texture.texture.format = halp::gpu_texture::RGBA32UI; break;

    // Signed integer — Qt 6.10+
    case QRhiTexture::R8SI:     outputs.texture.texture.format = halp::gpu_texture::R8SI;     break;
    case QRhiTexture::R32SI:    outputs.texture.texture.format = halp::gpu_texture::R32SI;    break;
    case QRhiTexture::RG32SI:   outputs.texture.texture.format = halp::gpu_texture::RG32SI;   break;
    case QRhiTexture::RGBA32SI: outputs.texture.texture.format = halp::gpu_texture::RGBA32SI; break;
#endif

    default:
      // Depth, compressed, or anything halp's enum doesn't cover —
      // safest fallback is RGBA8 so the downstream sampler binding
      // doesn't trip a type-mismatch validation error. Downstream
      // explicit consumers should branch on `kind` first.
      outputs.texture.texture.format = halp::gpu_texture::RGBA8;
      break;
  }
}

void ExtractTexture::release(score::gfx::RenderList& /*r*/)
{
  m_lastHandle = nullptr;
  m_lastName.clear();
  outputs.texture.texture.handle = nullptr;
  outputs.texture.texture.width = 0;
  outputs.texture.texture.height = 0;
  outputs.texture.texture.layers_or_depth = 1;
  outputs.texture.texture.kind = halp::texture_kind::texture_2d;
}

} // namespace Threedim
