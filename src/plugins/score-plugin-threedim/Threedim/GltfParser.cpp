#include "GltfParser.hpp"

#include "TangentUtils.hpp"

#include <ossia/detail/hash.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <QMatrix3x3>
#include <QQuaternion>
#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <variant>

namespace Threedim
{

namespace
{

// glTF TRS decomposition. With Options::DecomposeNodeMatrices we get TRS
// directly; otherwise we'd need to decompose the 4x4. fastgltf gives us a
// std::variant<TRS, fmat4x4> — handle both paths.
static ossia::scene_transform to_transform(const fastgltf::Node& n)
{
  ossia::scene_transform t{};

  if(const auto* trs = std::get_if<fastgltf::TRS>(&n.transform))
  {
    t.translation[0] = float(trs->translation[0]);
    t.translation[1] = float(trs->translation[1]);
    t.translation[2] = float(trs->translation[2]);
    t.rotation[0]    = float(trs->rotation[0]);
    t.rotation[1]    = float(trs->rotation[1]);
    t.rotation[2]    = float(trs->rotation[2]);
    t.rotation[3]    = float(trs->rotation[3]);
    t.scale[0]       = float(trs->scale[0]);
    t.scale[1]       = float(trs->scale[1]);
    t.scale[2]       = float(trs->scale[2]);
  }
  else if(const auto* m = std::get_if<fastgltf::math::fmat4x4>(&n.transform))
  {
    // Matrix form — full TRS decomposition. We pass
    // Options::DecomposeNodeMatrices so fastgltf SHOULD have already
    // converted to TRS upfront, but this branch still fires for
    // matrices that fastgltf flags as non-decomposable (negative
    // scale, near-degenerate, library version differences). The
    // previous translation-only fallback silently dropped rotation
    // and scale, which broke any glTF authored matrix-only — like
    // VirtualCity (193/234 nodes use matrix form encoding rotation
    // and uniform scale).
    //
    // Algorithm: T = column 3; per-column lengths give scale; reflect
    // one axis when det < 0; normalised 3×3 → quaternion via the
    // standard branch-on-trace method.
    const auto& M = *m;
    t.translation[0] = M[3][0];
    t.translation[1] = M[3][1];
    t.translation[2] = M[3][2];

    QVector3D c0(M[0][0], M[0][1], M[0][2]);
    QVector3D c1(M[1][0], M[1][1], M[1][2]);
    QVector3D c2(M[2][0], M[2][1], M[2][2]);

    float sx = c0.length();
    float sy = c1.length();
    float sz = c2.length();

    // Flip one axis when determinant is negative (reflection encoded
    // as negative scale on one axis). Without this, the quaternion
    // extraction below trips on a left-handed basis and yields garbage.
    const float det
        = c0.x() * (c1.y() * c2.z() - c1.z() * c2.y())
        - c0.y() * (c1.x() * c2.z() - c1.z() * c2.x())
        + c0.z() * (c1.x() * c2.y() - c1.y() * c2.x());
    if(det < 0.f)
    {
      sx = -sx;
      c0 = -c0;
    }

    t.scale[0] = sx;
    t.scale[1] = sy;
    t.scale[2] = sz;

    if(sx > 1e-6f) c0 /= sx;
    if(sy > 1e-6f) c1 /= sy;
    if(sz > 1e-6f) c2 /= sz;

    QMatrix3x3 R;
    R(0, 0) = c0.x(); R(1, 0) = c0.y(); R(2, 0) = c0.z();
    R(0, 1) = c1.x(); R(1, 1) = c1.y(); R(2, 1) = c1.z();
    R(0, 2) = c2.x(); R(1, 2) = c2.y(); R(2, 2) = c2.z();
    QQuaternion q = QQuaternion::fromRotationMatrix(R);
    t.rotation[0] = q.x();
    t.rotation[1] = q.y();
    t.rotation[2] = q.z();
    t.rotation[3] = q.scalar();
  }
  return t;
}

// Translate a glTF Material into material_component (factors + base color
// texture path). `dir` is the glTF file's parent directory — external
// image URIs are relative to it.
static std::shared_ptr<ossia::material_component> to_material(
    const fastgltf::Asset& asset, const fastgltf::Material& m,
    const std::filesystem::path& dir)
{
  auto mc = std::make_shared<ossia::material_component>();
  mc->tag = std::string(m.name);

  // Base color (pbrMetallicRoughness factor + texture)
  mc->base_color_factor[0] = float(m.pbrData.baseColorFactor[0]);
  mc->base_color_factor[1] = float(m.pbrData.baseColorFactor[1]);
  mc->base_color_factor[2] = float(m.pbrData.baseColorFactor[2]);
  mc->base_color_factor[3] = float(m.pbrData.baseColorFactor[3]);
  mc->metallic_factor   = float(m.pbrData.metallicFactor);
  mc->roughness_factor  = float(m.pbrData.roughnessFactor);

  mc->emissive_factor[0] = float(m.emissiveFactor[0]);
  mc->emissive_factor[1] = float(m.emissiveFactor[1]);
  mc->emissive_factor[2] = float(m.emissiveFactor[2]);
  mc->emissive_strength  = float(m.emissiveStrength);

  switch(m.alphaMode)
  {
    case fastgltf::AlphaMode::Opaque: mc->alpha = ossia::alpha_mode::opaque_; break;
    case fastgltf::AlphaMode::Mask:   mc->alpha = ossia::alpha_mode::mask;    break;
    case fastgltf::AlphaMode::Blend:  mc->alpha = ossia::alpha_mode::blend;   break;
  }
  mc->alpha_cutoff = float(m.alphaCutoff);
  mc->double_sided = m.doubleSided;
  mc->unlit = m.unlit;

  // Resolve a glTF texture slot to an ossia texture_ref with source populated
  // (filesystem path or embedded blob). The image may be external (URI), a
  // buffer view into the main glTF buffer, or an inline array.
  auto fill_tex = [&](ossia::texture_ref& tr, const fastgltf::TextureInfo& ti) {
    if(ti.textureIndex >= asset.textures.size())
      return;
    const auto& tex = asset.textures[ti.textureIndex];
    if(!tex.imageIndex.has_value())
      return;
    const auto& img = asset.images[tex.imageIndex.value()];
    auto src = std::make_shared<ossia::texture_source>();
    std::visit(
        [&](const auto& data) {
          using T = std::decay_t<decltype(data)>;
          if constexpr(std::is_same_v<T, fastgltf::sources::URI>)
          {
            // Relative URI → join with the glTF file's parent dir.
            // Contain the result inside that dir: URIs come from the
            // asset file, and "../" chains, absolute paths or symlinks
            // would let a shared scene read arbitrary local files.
            // weakly_canonical resolves symlinks; an empty base dir
            // (bare-filename load) can't be contained, so reject.
            std::error_code ec;
            const auto base = std::filesystem::weakly_canonical(dir, ec);
            if(ec || base.empty())
              return;
            const auto p = std::filesystem::weakly_canonical(
                dir / std::filesystem::path(std::string_view(data.uri.path())),
                ec);
            if(ec)
              return;
            auto [bEnd, pIt] = std::mismatch(
                base.begin(), base.end(), p.begin(), p.end());
            if(bEnd == base.end())
              src->file_path = p.string();
          }
          else if constexpr(std::is_same_v<T, fastgltf::sources::Array>)
          {
            auto blob = std::make_shared<std::vector<uint8_t>>(
                (const uint8_t*)data.bytes.data(),
                (const uint8_t*)data.bytes.data() + data.bytes.size());
            src->embedded_data = blob;
            src->mime_type = std::string(fastgltf::getMimeTypeString(data.mimeType));
          }
          else if constexpr(std::is_same_v<T, fastgltf::sources::BufferView>)
          {
            if(data.bufferViewIndex >= asset.bufferViews.size())
              return;
            const auto& bv = asset.bufferViews[data.bufferViewIndex];
            if(bv.bufferIndex >= asset.buffers.size())
              return;
            const auto& buf = asset.buffers[bv.bufferIndex];
            const auto* arr = std::get_if<fastgltf::sources::Array>(&buf.data);
            if(!arr)
              return;
            auto blob = std::make_shared<std::vector<uint8_t>>(
                (const uint8_t*)arr->bytes.data() + bv.byteOffset,
                (const uint8_t*)arr->bytes.data() + bv.byteOffset + bv.byteLength);
            src->embedded_data = blob;
            src->mime_type = std::string(fastgltf::getMimeTypeString(data.mimeType));
          }
          // sources::Vector / sources::Fallback / sources::CustomBuffer not
          // handled in v1 — most files use one of the three above.
        },
        img.data);

    // Plan 09 S1: content-hash for cross-output / cross-reload decode
    // dedup. Prefer hashing the embedded bytes — it's the decoded
    // payload contents that matter, not the file path (two different
    // files can embed the same JPEG). Fall back to hashing the path
    // string when no embedded data (URI → we'll read the file on
    // demand inside the preprocessor, hashing the path is a stable
    // proxy for session-scope dedup).
    if(src->embedded_data && !src->embedded_data->empty())
    {
      src->content_hash = ossia::hash_bytes(
          src->embedded_data->data(), src->embedded_data->size());
    }
    else if(!src->file_path.empty())
    {
      src->content_hash = ossia::hash_bytes(
          src->file_path.data(), src->file_path.size());
    }

    tr.source = std::move(src);
    tr.texcoord_set = uint32_t(ti.texCoordIndex);

    // KHR_texture_transform: per-texture-info UV transform. The
    // extension overrides the texture-info texCoordIndex when set
    // (spec) — honour that. Defaults are identity (offset=0, scale=1,
    // rot=0), so leaving uv_transform at default for textures without
    // the extension is correct.
    if(ti.transform)
    {
      tr.uv_transform.offset[0] = float(ti.transform->uvOffset.x());
      tr.uv_transform.offset[1] = float(ti.transform->uvOffset.y());
      tr.uv_transform.scale[0]  = float(ti.transform->uvScale.x());
      tr.uv_transform.scale[1]  = float(ti.transform->uvScale.y());
      tr.uv_transform.rotation  = float(ti.transform->rotation);
      if(ti.transform->texCoordIndex.has_value())
        tr.texcoord_set = uint32_t(*ti.transform->texCoordIndex);
    }

    // glTF per-texture sampler. Each texture optionally references a
    // sampler index in `asset.samplers`. Default (when absent or
    // unreferenced) is REPEAT/REPEAT/LINEAR/LINEAR/LINEAR_MIPMAP per
    // glTF spec — which matches the texture_sampler_config defaults.
    auto wrap_to_ossia = [](fastgltf::Wrap w) {
      switch(w)
      {
        case fastgltf::Wrap::ClampToEdge:    return ossia::CLAMP_TO_EDGE;
        case fastgltf::Wrap::MirroredRepeat: return ossia::MIRROR;
        case fastgltf::Wrap::Repeat:         return ossia::REPEAT;
      }
      return ossia::REPEAT;
    };
    auto filter_to_ossia = [](fastgltf::Filter f, ossia::texture_filter& base,
                              ossia::texture_filter& mip) {
      // glTF combined min-filter encodes both the base filter and the
      // mipmap mode (e.g. LinearMipMapNearest = LINEAR base + NEAREST
      // mipmap). Decode both axes.
      switch(f)
      {
        case fastgltf::Filter::Nearest:
          base = ossia::NEAREST; mip = ossia::NONE; break;
        case fastgltf::Filter::Linear:
          base = ossia::LINEAR;  mip = ossia::NONE; break;
        case fastgltf::Filter::NearestMipMapNearest:
          base = ossia::NEAREST; mip = ossia::NEAREST; break;
        case fastgltf::Filter::LinearMipMapNearest:
          base = ossia::LINEAR;  mip = ossia::NEAREST; break;
        case fastgltf::Filter::NearestMipMapLinear:
          base = ossia::NEAREST; mip = ossia::LINEAR; break;
        case fastgltf::Filter::LinearMipMapLinear:
          base = ossia::LINEAR;  mip = ossia::LINEAR; break;
      }
    };
    if(tex.samplerIndex.has_value()
       && *tex.samplerIndex < asset.samplers.size())
    {
      const auto& s = asset.samplers[*tex.samplerIndex];
      tr.sampler.wrap_s = wrap_to_ossia(s.wrapS);
      tr.sampler.wrap_t = wrap_to_ossia(s.wrapT);
      ossia::texture_filter mag_base = ossia::LINEAR, mag_mip = ossia::NONE;
      ossia::texture_filter min_base = ossia::LINEAR, min_mip = ossia::LINEAR;
      if(s.magFilter.has_value())
        filter_to_ossia(*s.magFilter, mag_base, mag_mip);
      if(s.minFilter.has_value())
        filter_to_ossia(*s.minFilter, min_base, min_mip);
      tr.sampler.mag_filter = mag_base;
      tr.sampler.min_filter = min_base;
      tr.sampler.mipmap_mode = min_mip;  // mip mode comes from minFilter
    }
  };

  if(m.pbrData.baseColorTexture)
    fill_tex(mc->base_color_texture, *m.pbrData.baseColorTexture);
  if(m.pbrData.metallicRoughnessTexture)
    fill_tex(mc->metallic_roughness_texture, *m.pbrData.metallicRoughnessTexture);
  if(m.normalTexture)
    fill_tex(mc->normal_texture, *m.normalTexture);
  if(m.occlusionTexture)
    fill_tex(mc->occlusion_texture, *m.occlusionTexture);
  if(m.emissiveTexture)
    fill_tex(mc->emissive_texture, *m.emissiveTexture);

  // --- KHR material extensions ------------------------------------------
  // fastgltf parses every extension we've enabled in the Extensions mask
  // at parse time (see loadFromFile() below). What was missing here is NOT
  // the parse — fastgltf already gave us the fields — but the copy into
  // ossia::material_component. Each KHR_* that sets material_component
  // fields gets a matching block below.
  //
  // Each material.<extension> is a unique_ptr; nullptr means the file
  // didn't declare that extension on this material. We leave the
  // material_component sub-struct at its spec defaults (factor=0 /
  // factor=1 depending on the field) in that case.

  // KHR_materials_ior — scalar IOR override; default 1.5 matches spec.
  mc->ior = float(m.ior);

  // KHR_materials_clearcoat — second thin dielectric specular lobe.
  if(m.clearcoat)
  {
    mc->clearcoat.factor = float(m.clearcoat->clearcoatFactor);
    mc->clearcoat.roughness_factor = float(m.clearcoat->clearcoatRoughnessFactor);
    if(m.clearcoat->clearcoatTexture)
      fill_tex(mc->clearcoat.texture, *m.clearcoat->clearcoatTexture);
    if(m.clearcoat->clearcoatRoughnessTexture)
      fill_tex(
          mc->clearcoat.roughness_texture,
          *m.clearcoat->clearcoatRoughnessTexture);
    if(m.clearcoat->clearcoatNormalTexture)
      fill_tex(
          mc->clearcoat.normal_texture, *m.clearcoat->clearcoatNormalTexture);
  }

  // KHR_materials_sheen — fabric / velvet / brushed surfaces.
  if(m.sheen)
  {
    mc->sheen.color_factor[0] = float(m.sheen->sheenColorFactor.x());
    mc->sheen.color_factor[1] = float(m.sheen->sheenColorFactor.y());
    mc->sheen.color_factor[2] = float(m.sheen->sheenColorFactor.z());
    mc->sheen.roughness_factor = float(m.sheen->sheenRoughnessFactor);
    if(m.sheen->sheenColorTexture)
      fill_tex(mc->sheen.color_texture, *m.sheen->sheenColorTexture);
    if(m.sheen->sheenRoughnessTexture)
      fill_tex(mc->sheen.roughness_texture, *m.sheen->sheenRoughnessTexture);
  }

  // KHR_materials_transmission — thin-walled refraction weight.
  if(m.transmission)
  {
    mc->transmission.factor = float(m.transmission->transmissionFactor);
    if(m.transmission->transmissionTexture)
      fill_tex(mc->transmission.texture, *m.transmission->transmissionTexture);
  }

  // KHR_materials_volume — thick-walled absorption + attenuation.
  if(m.volume)
  {
    mc->volume.thickness_factor = float(m.volume->thicknessFactor);
    mc->volume.attenuation_distance = float(m.volume->attenuationDistance);
    mc->volume.attenuation_color[0] = float(m.volume->attenuationColor.x());
    mc->volume.attenuation_color[1] = float(m.volume->attenuationColor.y());
    mc->volume.attenuation_color[2] = float(m.volume->attenuationColor.z());
    if(m.volume->thicknessTexture)
      fill_tex(mc->volume.thickness_texture, *m.volume->thicknessTexture);
  }

  // KHR_materials_specular — dielectric F0 override + tint.
  if(m.specular)
  {
    mc->specular.factor = float(m.specular->specularFactor);
    mc->specular.color_factor[0] = float(m.specular->specularColorFactor.x());
    mc->specular.color_factor[1] = float(m.specular->specularColorFactor.y());
    mc->specular.color_factor[2] = float(m.specular->specularColorFactor.z());
    if(m.specular->specularTexture)
      fill_tex(mc->specular.texture, *m.specular->specularTexture);
    if(m.specular->specularColorTexture)
      fill_tex(mc->specular.color_texture, *m.specular->specularColorTexture);
  }

  // KHR_materials_iridescence — thin-film interference.
  if(m.iridescence)
  {
    mc->iridescence.factor = float(m.iridescence->iridescenceFactor);
    mc->iridescence.ior = float(m.iridescence->iridescenceIor);
    mc->iridescence.thickness_min
        = float(m.iridescence->iridescenceThicknessMinimum);
    mc->iridescence.thickness_max
        = float(m.iridescence->iridescenceThicknessMaximum);
    if(m.iridescence->iridescenceTexture)
      fill_tex(mc->iridescence.texture, *m.iridescence->iridescenceTexture);
    if(m.iridescence->iridescenceThicknessTexture)
      fill_tex(
          mc->iridescence.thickness_texture,
          *m.iridescence->iridescenceThicknessTexture);
  }

  // KHR_materials_anisotropy — directional specular stretch.
  if(m.anisotropy)
  {
    mc->anisotropy.strength = float(m.anisotropy->anisotropyStrength);
    mc->anisotropy.rotation = float(m.anisotropy->anisotropyRotation);
    if(m.anisotropy->anisotropyTexture)
      fill_tex(mc->anisotropy.texture, *m.anisotropy->anisotropyTexture);
  }

  // KHR_materials_diffuse_transmission — translucent surfaces (paper,
  // leaves, lampshades).
  if(m.diffuseTransmission)
  {
    mc->diffuse_transmission.factor
        = float(m.diffuseTransmission->diffuseTransmissionFactor);
    mc->diffuse_transmission.color_factor[0]
        = float(m.diffuseTransmission->diffuseTransmissionColorFactor.x());
    mc->diffuse_transmission.color_factor[1]
        = float(m.diffuseTransmission->diffuseTransmissionColorFactor.y());
    mc->diffuse_transmission.color_factor[2]
        = float(m.diffuseTransmission->diffuseTransmissionColorFactor.z());
    if(m.diffuseTransmission->diffuseTransmissionTexture)
      fill_tex(
          mc->diffuse_transmission.texture,
          *m.diffuseTransmission->diffuseTransmissionTexture);
    if(m.diffuseTransmission->diffuseTransmissionColorTexture)
      fill_tex(
          mc->diffuse_transmission.color_texture,
          *m.diffuseTransmission->diffuseTransmissionColorTexture);
  }

  return mc;
}

// Translate a glTF Light (KHR_lights_punctual) to ossia::light_component.
static std::shared_ptr<ossia::light_component> to_light(const fastgltf::Light& l)
{
  auto lc = std::make_shared<ossia::light_component>();
  switch(l.type)
  {
    case fastgltf::LightType::Directional:
      lc->type = ossia::light_type::directional; break;
    case fastgltf::LightType::Point:
      lc->type = ossia::light_type::point; break;
    case fastgltf::LightType::Spot:
      lc->type = ossia::light_type::spot; break;
  }
  lc->color[0]  = float(l.color[0]);
  lc->color[1]  = float(l.color[1]);
  lc->color[2]  = float(l.color[2]);
  lc->intensity = float(l.intensity);
  lc->range     = l.range.value_or(0.f);
  lc->inner_cone_angle = float(l.innerConeAngle.value_or(0.f));
  lc->outer_cone_angle = float(l.outerConeAngle.value_or(float(M_PI) / 4.f));
  lc->decay = ossia::light_decay::quadratic;
  return lc;
}

// Translate a glTF Camera.
static std::shared_ptr<ossia::camera_component> to_camera(const fastgltf::Camera& c)
{
  auto cc = std::make_shared<ossia::camera_component>();
  if(const auto* p = std::get_if<fastgltf::Camera::Perspective>(&c.camera))
  {
    cc->projection   = ossia::camera_projection::perspective;
    cc->yfov         = float(p->yfov);
    cc->aspect_ratio = p->aspectRatio.value_or(1.f);
    cc->znear        = float(p->znear);
    cc->zfar         = float(p->zfar.value_or(1000.f));
  }
  else if(const auto* o = std::get_if<fastgltf::Camera::Orthographic>(&c.camera))
  {
    cc->projection = ossia::camera_projection::orthographic;
    cc->xmag  = float(o->xmag);
    cc->ymag  = float(o->ymag);
    cc->znear = float(o->znear);
    cc->zfar  = float(o->zfar);
  }
  return cc;
}

// ---------------------------------------------------------------------------
// Accessor bounds validation.
//
// fastgltf::validate() checks an accessor's bufferView *index*, component
// alignment and count >= 1, but it never verifies that
//   accessor.byteOffset + (count - 1) * stride + elementSize
//     <= bufferView.byteLength
// nor that the bufferView itself fits inside its buffer's actual bytes.
// A hostile file declaring e.g. "count": 30000 against a 102-byte
// bufferView would therefore drive fastgltf::iterateAccessor into an
// out-of-bounds heap read. Every accessor must pass these checks before
// being iterated.

// Actual number of bytes backing a buffer, from the loaded data source —
// the declared buffer.byteLength is file-controlled and may lie.
static std::size_t buffer_source_byte_size(const fastgltf::Buffer& buffer)
{
  return std::visit(
      fastgltf::visitor{
          [](const auto&) -> std::size_t {
            // URI / CustomBuffer / Fallback / BufferView sources are not
            // produced for buffers parsed with LoadExternalBuffers; treat
            // them as unreadable so dependent accessors get rejected
            // rather than trusting a declared byteLength.
            return 0;
          },
          [](const fastgltf::sources::Array& a) -> std::size_t
          { return a.bytes.size_bytes(); },
          [](const fastgltf::sources::Vector& v) -> std::size_t
          { return v.bytes.size(); },
          [](const fastgltf::sources::ByteView& b) -> std::size_t
          { return b.bytes.size_bytes(); },
      },
      buffer.data);
}

// True iff reading `count` elements of `elem_read_size` bytes each, spaced
// `stride` bytes apart, starting `byte_offset` bytes into bufferView
// `view_idx`, stays within both the view and its backing buffer bytes.
// All arithmetic is overflow-safe (guarded subtractions + division; no
// unchecked file-controlled multiplication).
static bool range_within_view(
    const fastgltf::Asset& asset, std::size_t view_idx, std::size_t byte_offset,
    std::size_t count, std::size_t stride, std::size_t elem_read_size)
{
  if(count == 0)
    return true;
  if(stride == 0 || elem_read_size == 0)
    return false;
  if(view_idx >= asset.bufferViews.size())
    return false;
  const auto& view = asset.bufferViews[view_idx];
  if(view.bufferIndex >= asset.buffers.size())
    return false;

  // The view must fit in the buffer's actually-loaded bytes.
  const std::size_t buf_size
      = buffer_source_byte_size(asset.buffers[view.bufferIndex]);
  if(view.byteLength > buf_size || view.byteOffset > buf_size - view.byteLength)
    return false;

  // First element: [byte_offset, byte_offset + elem_read_size) must fit.
  if(byte_offset > view.byteLength
     || elem_read_size > view.byteLength - byte_offset)
    return false;
  // Remaining count - 1 strides must fit in what's left.
  const std::size_t remaining = view.byteLength - byte_offset - elem_read_size;
  return count - 1 <= remaining / stride;
}

// `iter_components` is the number of components the caller will actually
// decode per element; fastgltf's accessor iterators read that many
// regardless of the accessor's own declared type, so a type-mismatched
// accessor must be bounded by the *wider* of the two.
static bool accessor_within_bounds(
    const fastgltf::Asset& asset, const fastgltf::Accessor& acc,
    std::size_t iter_components)
{
  const std::size_t comp_size = fastgltf::getComponentByteSize(acc.componentType);
  const std::size_t packed_size
      = fastgltf::getElementByteSize(acc.type, acc.componentType);
  if(comp_size == 0 || packed_size == 0)
    return false;
  const std::size_t read_size = std::max(packed_size, iter_components * comp_size);

  if(acc.bufferViewIndex.has_value())
  {
    const std::size_t view_idx = *acc.bufferViewIndex;
    if(view_idx >= asset.bufferViews.size())
      return false;
    const std::size_t stride
        = asset.bufferViews[view_idx].byteStride.value_or(packed_size);
    if(!range_within_view(asset, view_idx, acc.byteOffset, acc.count, stride, read_size))
      return false;
  }
  if(acc.sparse.has_value())
  {
    const auto& sp = *acc.sparse;
    // The sparse substitution streams hold at most `count` entries each,
    // consumed while iterating the base accessor — they may not exceed it.
    if(sp.count > acc.count)
      return false;
    const std::size_t idx_size = fastgltf::getElementByteSize(
        fastgltf::AccessorType::Scalar, sp.indexComponentType);
    if(idx_size == 0)
      return false;
    // Sparse bufferViews cannot declare a byteStride (glTF spec); both
    // streams are tightly packed.
    if(!range_within_view(
           asset, sp.indicesBufferView, sp.indicesByteOffset, sp.count,
           idx_size, idx_size))
      return false;
    if(!range_within_view(
           asset, sp.valuesBufferView, sp.valuesByteOffset, sp.count,
           packed_size, read_size))
      return false;
  }
  return true;
}

// Sweep every accessor in the asset once at load time, with its declared
// element width. Covers all downstream consumers in one pass; read sites
// additionally re-check with the width they actually iterate.
static bool all_accessors_within_bounds(const fastgltf::Asset& asset)
{
  for(const auto& acc : asset.accessors)
    if(!accessor_within_bounds(asset, acc, fastgltf::getNumComponents(acc.type)))
      return false;
  return true;
}

// Pull one accessor into a float vector. `components` is the number of
// floats per element (1/2/3/4). fastgltf's iterator handles all component
// types (byte/short/int/float) with automatic widening to float.
// Returns nullptr when the accessor's data range escapes its bufferView.
template <int Components>
static std::shared_ptr<std::vector<float>> read_float_accessor(
    const fastgltf::Asset& asset, const fastgltf::Accessor& acc)
{
  if(!accessor_within_bounds(asset, acc, Components))
    return {};
  auto out = std::make_shared<std::vector<float>>(acc.count * Components);
  float* dst = out->data();
  if constexpr(Components == 2)
  {
    fastgltf::iterateAccessor<fastgltf::math::fvec2>(
        asset, acc, [&](fastgltf::math::fvec2 v) {
          dst[0] = v.x(); dst[1] = v.y(); dst += 2;
        });
  }
  else if constexpr(Components == 3)
  {
    fastgltf::iterateAccessor<fastgltf::math::fvec3>(
        asset, acc, [&](fastgltf::math::fvec3 v) {
          dst[0] = v.x(); dst[1] = v.y(); dst[2] = v.z(); dst += 3;
        });
  }
  else if constexpr(Components == 4)
  {
    fastgltf::iterateAccessor<fastgltf::math::fvec4>(
        asset, acc, [&](fastgltf::math::fvec4 v) {
          dst[0] = v.x(); dst[1] = v.y(); dst[2] = v.z(); dst[3] = v.w(); dst += 4;
        });
  }
  return out;
}

// Pull indices (whatever the glTF component type) into a flat uint32 buffer.
// Returns nullptr when the accessor's data range escapes its bufferView.
static std::shared_ptr<std::vector<uint32_t>> read_indices(
    const fastgltf::Asset& asset, const fastgltf::Accessor& acc)
{
  if(!accessor_within_bounds(asset, acc, 1))
    return {};
  auto out = std::make_shared<std::vector<uint32_t>>(acc.count);
  uint32_t* dst = out->data();
  fastgltf::iterateAccessor<std::uint32_t>(
      asset, acc, [&](std::uint32_t v) { *dst++ = v; });
  return out;
}

// Pull POSITION, NORMAL, TEXCOORD_0, COLOR_0, TANGENT for a primitive into a
// ScenePart. Missing attributes leave the matching shared_ptr empty.
static GltfParser::ScenePart extract_primitive(
    const fastgltf::Asset& asset, const fastgltf::Primitive& prim,
    const std::vector<int>& material_index_remap)
{
  GltfParser::ScenePart sp;

  auto get_accessor
      = [&](std::string_view name) -> const fastgltf::Accessor* {
    for(const auto& a : prim.attributes)
      if(a.name == name && a.accessorIndex < asset.accessors.size())
        return &asset.accessors[a.accessorIndex];
    return nullptr;
  };

  if(auto* a = get_accessor("POSITION"))
  {
    sp.positions = read_float_accessor<3>(asset, *a);
    if(!sp.positions)
    {
      qDebug() << "GltfParser: POSITION accessor out of bounds, "
                  "skipping primitive";
      return {};
    }
    sp.vertex_count = uint32_t(a->count);
    // Local-space AABB. glTF requires min/max on the POSITION accessor,
    // but rather than chase fastgltf's accessor-specific variant API we
    // just walk the decoded float stream — same cost as one extra pass
    // on load (negligible compared to asset I/O), and trivially uniform
    // with the FBX / procedural code paths.
    if(!sp.positions->empty())
      sp.bounds = ossia::compute_aabb_from_positions(
          sp.positions->data(), sp.vertex_count);
  }
  if(auto* a = get_accessor("NORMAL"))
    sp.normals = read_float_accessor<3>(asset, *a);
  if(auto* a = get_accessor("TEXCOORD_0"))
    sp.texcoords = read_float_accessor<2>(asset, *a);
  if(auto* a = get_accessor("TEXCOORD_1"))
    sp.texcoords1 = read_float_accessor<2>(asset, *a);
  if(auto* a = get_accessor("COLOR_0"))
  {
    // COLOR_0 may be vec3 or vec4 — peek at component count.
    if(a->type == fastgltf::AccessorType::Vec4)
      sp.colors = read_float_accessor<4>(asset, *a);
    else if(a->type == fastgltf::AccessorType::Vec3)
    {
      // Pad to RGBA.
      if(auto rgb = read_float_accessor<3>(asset, *a))
      {
        auto rgba = std::make_shared<std::vector<float>>(a->count * 4);
        for(std::size_t i = 0; i < a->count; ++i)
        {
          (*rgba)[i * 4 + 0] = (*rgb)[i * 3 + 0];
          (*rgba)[i * 4 + 1] = (*rgb)[i * 3 + 1];
          (*rgba)[i * 4 + 2] = (*rgb)[i * 3 + 2];
          (*rgba)[i * 4 + 3] = 1.f;
        }
        sp.colors = std::move(rgba);
      }
    }
  }
  if(auto* a = get_accessor("TANGENT"))
    sp.tangents = read_float_accessor<4>(asset, *a);

  // Skinning attributes. glTF spec stores JOINTS_0 as UNSIGNED_BYTE or
  // UNSIGNED_SHORT vec4 — widen to uint32 here so the vertex shader can
  // bind a uniform uvec4 format regardless of source file. WEIGHTS_0 is
  // always float vec4 per glTF normative spec.
  if(auto* a = get_accessor("JOINTS_0");
     a && accessor_within_bounds(asset, *a, 4))
  {
    auto joints = std::make_shared<std::vector<uint32_t>>(a->count * 4);
    uint32_t* dst = joints->data();
    fastgltf::iterateAccessor<fastgltf::math::u16vec4>(
        asset, *a, [&](fastgltf::math::u16vec4 v) {
          *dst++ = uint32_t(v[0]);
          *dst++ = uint32_t(v[1]);
          *dst++ = uint32_t(v[2]);
          *dst++ = uint32_t(v[3]);
        });
    sp.joints0 = std::move(joints);
  }
  if(auto* a = get_accessor("WEIGHTS_0"))
    sp.weights0 = read_float_accessor<4>(asset, *a);

  if(prim.indicesAccessor.has_value())
  {
    if(*prim.indicesAccessor >= asset.accessors.size())
    {
      qDebug() << "GltfParser: index accessor index out of range, "
                  "skipping primitive";
      return {};
    }
    const auto& ia = asset.accessors[*prim.indicesAccessor];
    sp.indices = read_indices(asset, ia);
    if(!sp.indices)
    {
      qDebug() << "GltfParser: index accessor out of bounds, "
                  "skipping primitive";
      return {};
    }
    sp.index_count = uint32_t(ia.count);
  }

  if(prim.materialIndex.has_value())
  {
    const std::size_t gltf_idx = *prim.materialIndex;
    if(gltf_idx < material_index_remap.size())
      sp.material_index = material_index_remap[gltf_idx];
  }

  // KHR_materials_variants mapping. fastgltf stores it pre-indexed by
  // variant index → Optional<material_index>. Translate to our
  // remapped material indices with -1 for "no override".
  if(!prim.mappings.empty())
  {
    sp.variant_material_indices.resize(prim.mappings.size(), -1);
    for(std::size_t v = 0; v < prim.mappings.size(); ++v)
    {
      if(prim.mappings[v].has_value())
      {
        const std::size_t mi = *prim.mappings[v];
        if(mi < material_index_remap.size())
          sp.variant_material_indices[v] = material_index_remap[mi];
      }
    }
  }

  // Generate tangents via mikktspace when the glTF mesh didn't ship
  // them. Required for normal-mapped PBR: the fragment shader rebuilds
  // the TBN basis from (normal, tangent.xyz, cross(normal, tangent.xyz) *
  // tangent.w) before unpacking the sampled normal. Skipped when any
  // prerequisite stream is missing (no UVs → no normal mapping anyway).
  if(!sp.tangents && sp.positions && sp.normals && sp.texcoords)
  {
    sp.tangents = Threedim::generate_tangents_mikktspace(
        sp.positions, sp.normals, sp.texcoords, sp.indices,
        sp.vertex_count);
  }
  return sp;
}

// Convert a ScenePart to mesh_primitive (mirrors FbxParser::part_to_primitive
// but with index-buffer support — glTF exposes indexed meshes).
static ossia::buffer_resource_ptr make_buffer_resource_f(
    std::shared_ptr<std::vector<float>> floats)
{
  if(!floats || floats->empty())
    return {};
  auto br = std::make_shared<ossia::buffer_resource>();
  ossia::buffer_data bd;
  bd.data = std::shared_ptr<const void>(floats, floats->data());
  bd.byte_size = int64_t(floats->size() * sizeof(float));
  bd.usage_hint = ossia::buffer_data::usage::vertex_buffer;
  br->resource = std::move(bd);
  br->dirty_index = 1;
  return br;
}
static ossia::buffer_resource_ptr make_buffer_resource_u32(
    std::shared_ptr<std::vector<uint32_t>> ints)
{
  if(!ints || ints->empty())
    return {};
  auto br = std::make_shared<ossia::buffer_resource>();
  ossia::buffer_data bd;
  bd.data = std::shared_ptr<const void>(ints, ints->data());
  bd.byte_size = int64_t(ints->size() * sizeof(uint32_t));
  bd.usage_hint = ossia::buffer_data::usage::index_buffer;
  br->resource = std::move(bd);
  br->dirty_index = 1;
  return br;
}

static ossia::mesh_primitive part_to_primitive(
    const GltfParser::ScenePart& p,
    const std::vector<std::shared_ptr<ossia::material_component>>& mats)
{
  ossia::mesh_primitive mp;
  // Per-primitive id — not deterministic across reloads (part_to_primitive
  // is called from the scene walk where the source asset path isn't
  // threaded in), so mint a fresh id. Sessions with the same model file
  // reloaded will see different ids, which is acceptable: the preprocessor
  // rebuilds on material/mesh fingerprint changes anyway, and stable-id
  // stability is only critical for the material / transform fingerprints
  // which ARE deterministic via the file-path hash.
  mp.stable_id = ossia::mint_stable_id();
  mp.topology    = ossia::primitive_topology::triangles;
  mp.index_type  = p.indices ? ossia::index_format::uint32 : ossia::index_format::none;
  mp.vertex_count = p.vertex_count;
  mp.index_count  = p.index_count;
  mp.first_vertex = 0;
  mp.first_index  = 0;
  mp.vertex_offset = 0;
  mp.bounds = p.bounds;
  if(p.material_index >= 0
     && std::size_t(p.material_index) < mats.size())
    mp.material = mats[p.material_index];

  // KHR_materials_variants: per-variant material override. Index V
  // → null = "use default", else the variant's material_component_ptr.
  if(!p.variant_material_indices.empty())
  {
    mp.material_variants.resize(p.variant_material_indices.size());
    for(std::size_t v = 0; v < p.variant_material_indices.size(); ++v)
    {
      const int mi = p.variant_material_indices[v];
      if(mi >= 0 && std::size_t(mi) < mats.size())
        mp.material_variants[v]
            = ossia::material_component_ptr(mats[mi]);
    }
  }

  uint32_t bi = 0;
  auto add = [&](auto buf, int floats_per_vertex,
                 ossia::attribute_semantic sem, ossia::vertex_format fmt) {
    if(!buf || buf->empty())
      return;
    mp.vertex_buffers.push_back(make_buffer_resource_f(buf));
    ossia::vertex_attribute attr;
    attr.semantic = sem;
    attr.format = fmt;
    attr.buffer_index = bi++;
    attr.byte_offset = 0;
    attr.byte_stride = uint32_t(floats_per_vertex) * sizeof(float);
    attr.rate = ossia::vertex_attribute::input_rate::per_vertex;
    mp.attributes.push_back(attr);
  };

  add(p.positions, 3, ossia::attribute_semantic::position,  ossia::vertex_format::float3);
  add(p.normals,   3, ossia::attribute_semantic::normal,    ossia::vertex_format::float3);
  add(p.texcoords,  2, ossia::attribute_semantic::texcoord0, ossia::vertex_format::float2);
  add(p.texcoords1, 2, ossia::attribute_semantic::texcoord1, ossia::vertex_format::float2);
  add(p.colors,     4, ossia::attribute_semantic::color0,    ossia::vertex_format::float4);
  add(p.tangents,   4, ossia::attribute_semantic::tangent,   ossia::vertex_format::float4);

  // Skinning attributes — uvec4 joints + vec4 weights, one set per vertex.
  if(p.joints0)
  {
    auto br = std::make_shared<ossia::buffer_resource>();
    ossia::buffer_data bd;
    bd.data = std::shared_ptr<const void>(p.joints0, p.joints0->data());
    bd.byte_size = int64_t(p.joints0->size() * sizeof(uint32_t));
    bd.usage_hint = ossia::buffer_data::usage::vertex_buffer;
    br->resource = std::move(bd);
    br->dirty_index = 1;
    mp.vertex_buffers.push_back(std::move(br));
    ossia::vertex_attribute attr;
    attr.semantic = ossia::attribute_semantic::joints0;
    attr.format = ossia::vertex_format::uint32x4;
    attr.buffer_index = bi++;
    attr.byte_offset = 0;
    attr.byte_stride = 4 * sizeof(uint32_t);
    attr.rate = ossia::vertex_attribute::input_rate::per_vertex;
    mp.attributes.push_back(attr);
  }
  add(p.weights0, 4, ossia::attribute_semantic::weights0, ossia::vertex_format::float4);

  if(p.indices)
    mp.index_buffer = make_buffer_resource_u32(p.indices);

  return mp;
}

// Walk glTF scene hierarchy into FbxParser::SceneNode-like flat array.
// Returns the index of the emitted root-most parent for the given glTF node
// index, or -1 if unused.
static int emit_node(
    const fastgltf::Asset& asset, std::size_t nodeIdx, int parent_index,
    std::vector<GltfParser::SceneNode>& out,
    const std::vector<int>& material_index_remap,
    std::vector<char>& visited, int depth = 0)
{
  // Node indices come straight from the file; validate() checks neither
  // their range nor for cycles. Bound the index, cap depth, and mark
  // visited so a diamond/cyclic child graph can't re-expand the same
  // subtree exponentially or loop forever.
  if(depth > 256 || nodeIdx >= asset.nodes.size() || visited[nodeIdx])
    return -1;
  visited[nodeIdx] = 1;
  const auto& n = asset.nodes[nodeIdx];

  GltfParser::SceneNode sn;
  sn.name = std::string(n.name);
  sn.parent_index = parent_index;
  sn.local_transform = to_transform(n);
  // Stable ID = glTF node index + 1 (0 is the "unset" sentinel). Lets
  // AnimationPlayer and skeleton_component::joint_node_ids resolve
  // scene_node_id back to the emitted node.
  sn.stable_id = std::uint64_t(nodeIdx) + 1;

  // glTF skin association — when the node references a skin, stamp its
  // index so the downstream mesh_component inherits it.
  if(n.skinIndex.has_value())
    sn.skin_index = int32_t(*n.skinIndex);

  if(n.meshIndex.has_value())
  {
    const auto& mesh = asset.meshes[*n.meshIndex];
    sn.parts.reserve(mesh.primitives.size());
    for(const auto& prim : mesh.primitives)
    {
      auto sp = extract_primitive(asset, prim, material_index_remap);
      if(sp.vertex_count > 0)
        sn.parts.push_back(std::move(sp));
    }
  }
  if(n.lightIndex.has_value() && *n.lightIndex < asset.lights.size())
    sn.light = to_light(asset.lights[*n.lightIndex]);
  if(n.cameraIndex.has_value() && *n.cameraIndex < asset.cameras.size())
    sn.camera = to_camera(asset.cameras[*n.cameraIndex]);

  const int self = (int)out.size();
  out.push_back(std::move(sn));
  for(std::size_t ci : asset.nodes[nodeIdx].children)
    emit_node(asset, ci, self, out, material_index_remap, visited, depth + 1);
  return self;
}

}  // namespace

// =============================================================================
// rebuild_scene — same pattern as FbxParser::rebuild_scene.
// =============================================================================
void GltfParser::rebuild_scene()
{
  if(m_scene_nodes.empty())
    return;

  const std::size_t N = m_scene_nodes.size();
  std::vector<std::shared_ptr<ossia::scene_node>> nodes(N);
  std::vector<std::shared_ptr<std::vector<ossia::scene_payload>>> children(N);
  for(std::size_t i = 0; i < N; ++i)
  {
    nodes[i] = std::make_shared<ossia::scene_node>();
    nodes[i]->name = m_scene_nodes[i].name;
    nodes[i]->visible = true;
    nodes[i]->id.value = m_scene_nodes[i].stable_id;
    children[i] = std::make_shared<std::vector<ossia::scene_payload>>();
  }
  for(std::size_t i = 0; i < N; ++i)
  {
    auto& src = m_scene_nodes[i];
    auto& lst = *children[i];
    lst.push_back(src.local_transform);
    if(!src.parts.empty())
    {
      auto mc = std::make_shared<ossia::mesh_component>();
      mc->primitives.reserve(src.parts.size());
      for(const auto& p : src.parts)
        mc->primitives.push_back(part_to_primitive(p, m_materials));
      // Direct skeleton pointer (glTF node.skin index → m_skeletons).
      if(src.skin_index >= 0
         && std::size_t(src.skin_index) < m_skeletons.size())
        mc->skin = ossia::skeleton_component_ptr(m_skeletons[src.skin_index]);
      mc->dirty_index = 1;
      lst.push_back(ossia::mesh_component_ptr(std::move(mc)));
    }
    if(src.light)
      lst.push_back(ossia::light_component_ptr(src.light));
    if(src.camera)
      lst.push_back(ossia::camera_component_ptr(src.camera));
  }
  for(std::size_t i = 0; i < N; ++i)
  {
    int p = m_scene_nodes[i].parent_index;
    if(p >= 0 && p < (int)N)
      children[p]->push_back(ossia::scene_node_ptr(nodes[i]));
  }
  for(std::size_t i = 0; i < N; ++i)
    nodes[i]->children = children[i];

  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  for(std::size_t i = 0; i < N; ++i)
    if(m_scene_nodes[i].parent_index < 0)
      roots->push_back(ossia::scene_node_ptr(nodes[i]));

  auto mat_list = std::make_shared<std::vector<ossia::material_component_ptr>>();
  mat_list->reserve(m_materials.size());
  for(auto& m : m_materials)
    mat_list->push_back(ossia::material_component_ptr(m));

  auto state = std::make_shared<ossia::scene_state>();
  state->roots = std::move(roots);
  state->materials = std::move(mat_list);
  if(!m_skeletons.empty())
  {
    auto skel_list
        = std::make_shared<std::vector<ossia::skeleton_component_ptr>>();
    skel_list->reserve(m_skeletons.size());
    for(auto& s : m_skeletons)
      skel_list->push_back(ossia::skeleton_component_ptr(s));
    state->skeletons = std::move(skel_list);
  }
  state->version = 1;
  state->dirty_index = 1;

  // Expose asset-scope variant names for UI / controls. active_variant
  // starts at -1 (use each primitive's default material).
  if(!m_variant_names.empty())
  {
    state->variant_names.assign(
        m_variant_names.begin(), m_variant_names.end());
    state->active_variant_index = -1;
  }

  // AssetLoader wraps m_raw_state in a TRS payload externally; we
  // publish only the raw scene here.
  m_raw_state = std::move(state);
}

std::function<void(GltfParser&)> GltfParser::ins::gltf_t::process(file_type tv)
{
  if(tv.filename.empty())
    return {};

  const std::filesystem::path path(tv.filename);
  if(!std::filesystem::exists(path))
    return {};

  // Enable every extension we can usefully translate. Unknown required
  // extensions make fastgltf refuse the file; we intentionally enable more
  // than we consume to avoid that (data we don't translate is ignored).
  constexpr auto extensions =
      fastgltf::Extensions::KHR_mesh_quantization
      | fastgltf::Extensions::KHR_texture_transform
      | fastgltf::Extensions::KHR_lights_punctual
      | fastgltf::Extensions::KHR_materials_emissive_strength
      | fastgltf::Extensions::KHR_materials_unlit
      | fastgltf::Extensions::KHR_materials_ior
      | fastgltf::Extensions::KHR_materials_specular
      | fastgltf::Extensions::KHR_materials_transmission
      | fastgltf::Extensions::KHR_materials_volume
      | fastgltf::Extensions::KHR_materials_clearcoat
      | fastgltf::Extensions::KHR_materials_sheen
      | fastgltf::Extensions::KHR_materials_iridescence
      | fastgltf::Extensions::KHR_materials_anisotropy
      | fastgltf::Extensions::KHR_materials_diffuse_transmission
      | fastgltf::Extensions::KHR_materials_variants;

  fastgltf::Parser parser(extensions);

  constexpr auto gltfOptions
      = fastgltf::Options::DontRequireValidAssetMember
        | fastgltf::Options::AllowDouble
        | fastgltf::Options::LoadExternalBuffers
        | fastgltf::Options::LoadExternalImages
        | fastgltf::Options::GenerateMeshIndices
        | fastgltf::Options::DecomposeNodeMatrices;

  auto gltfFile = fastgltf::GltfDataBuffer::FromPath(path);
  if(!bool(gltfFile))
    return {};

  auto assetE = parser.loadGltf(
      gltfFile.get(), path.parent_path(), gltfOptions);
  if(assetE.error() != fastgltf::Error::None)
    return {};
  fastgltf::Asset asset = std::move(assetE.get());

  // The extraction below indexes accessors / meshes / skins straight from
  // file-provided indices. fastgltf::validate() bounds-checks most of the
  // *indices* but NOT the accessors' data ranges — it never verifies
  // accessor.byteOffset + count * elementSize <= bufferView.byteLength —
  // so a hostile accessor "count" would drive iterateAccessor into an
  // out-of-bounds heap read. Sweep every accessor's range first; running
  // before validate() also keeps validate() itself away from hostile
  // sparse-accessor bufferView indices, which it dereferences unchecked.
  if(!all_accessors_within_bounds(asset))
  {
    qDebug() << "GltfParser: accessor data range out of bounds, rejecting:"
             << path.string().c_str();
    return {};
  }
  if(fastgltf::validate(asset) != fastgltf::Error::None)
  {
    qDebug() << "GltfParser: asset failed validation:" << path.string().c_str();
    return {};
  }

  // Materials first so primitives can remap their material indices.
  std::vector<std::shared_ptr<ossia::material_component>> materials;
  std::vector<int> material_index_remap(asset.materials.size(), -1);
  for(std::size_t i = 0; i < asset.materials.size(); ++i)
  {
    material_index_remap[i] = (int)materials.size();
    auto mat = to_material(asset, asset.materials[i], path.parent_path());
    // Deterministic id keyed on (asset path, "mat", index) — re-reads of the
    // same asset file give the same material their same stable_id, so
    // downstream caches survive asset reloads.
    mat->stable_id = ossia::scene_node_id::from_parent(
        ossia::scene_node_id::from_path(path.string()),
        std::string("mat/") + std::to_string(i)).value;
    materials.push_back(std::move(mat));
  }

  // Scene — walk the default scene's roots. glTF allows multiple scenes but
  // only one is "active"; pick asset.defaultScene or the first.
  std::vector<GltfParser::SceneNode> scene_nodes;
  const std::size_t sceneIdx
      = asset.defaultScene.value_or(asset.scenes.empty() ? 0 : 0);
  if(sceneIdx < asset.scenes.size())
  {
    std::vector<char> visited(asset.nodes.size(), 0);
    for(std::size_t rootIdx : asset.scenes[sceneIdx].nodeIndices)
      emit_node(asset, rootIdx, -1, scene_nodes, material_index_remap, visited);
  }

  if(scene_nodes.empty())
    return {};

  // Skins — parse joint node list + inverse-bind matrices per skin.
  // Joint transforms themselves live on the scene_node's local_transform
  // (set during emit_node). AnimationPlayer consumes this skeleton data
  // to produce per-frame world-space joint matrices.
  std::vector<std::shared_ptr<ossia::skeleton_component>> skeletons;
  skeletons.reserve(asset.skins.size());
  for(const auto& sk : asset.skins)
  {
    auto skel = std::make_shared<ossia::skeleton_component>();
    // Inverse-bind matrices are optional in glTF; default is identity.
    std::vector<float> ibms;
    if(sk.inverseBindMatrices.has_value()
       && *sk.inverseBindMatrices < asset.accessors.size())
    {
      const auto& ibmAcc = asset.accessors[*sk.inverseBindMatrices];
      if(accessor_within_bounds(asset, ibmAcc, 16))
      {
        ibms.resize(ibmAcc.count * 16);
        std::size_t i = 0;
        fastgltf::iterateAccessor<fastgltf::math::fmat4x4>(
            asset, ibmAcc, [&](fastgltf::math::fmat4x4 m) {
              for(int c = 0; c < 4; ++c)
                for(int r = 0; r < 4; ++r)
                  ibms[i++] = m[c][r];
            });
      }
      else
      {
        qDebug() << "GltfParser: inverse-bind-matrix accessor out of "
                    "bounds, using identity";
      }
    }
    skel->joints.reserve(sk.joints.size());
    skel->joint_node_ids.reserve(sk.joints.size());
    for(std::size_t j = 0; j < sk.joints.size(); ++j)
    {
      ossia::skeleton_joint sj;
      const auto nodeIdx = sk.joints[j];
      if(nodeIdx < asset.nodes.size())
        sj.name = std::string(asset.nodes[nodeIdx].name);
      sj.parent_index = -1; // resolved from node hierarchy at use-time
      if(j * 16 + 15 < ibms.size())
        std::memcpy(
            sj.inverse_bind_matrix, ibms.data() + j * 16,
            sizeof(float) * 16);
      skel->joints.push_back(std::move(sj));
      // Stable node_id derived from the glTF node index (+1 because 0
      // means "unset" per scene_node_id convention). Matches the IDs
      // assigned to emitted scene_nodes in rebuild_scene below.
      ossia::scene_node_id nid;
      nid.value = std::uint64_t(nodeIdx) + 1;
      skel->joint_node_ids.push_back(nid);
    }
    skel->dirty_index = 1;
    skeletons.push_back(std::move(skel));
  }

  // KHR_materials_variants: asset-scope variant name list. Carried
  // alongside m_materials/skeletons into the parser so rebuild_scene
  // can copy it into scene_state. Capture the asset's materialVariants
  // by value so the lambda doesn't depend on the asset's lifetime.
  std::vector<std::string> variant_names(
      asset.materialVariants.begin(), asset.materialVariants.end());

  return [scene_nodes = std::move(scene_nodes),
          materials = std::move(materials),
          skeletons = std::move(skeletons),
          variant_names = std::move(variant_names)](GltfParser& o) mutable {
    std::swap(o.m_scene_nodes, scene_nodes);
    std::swap(o.m_materials, materials);
    std::swap(o.m_skeletons, skeletons);
    std::swap(o.m_variant_names, variant_names);
    o.rebuild_scene();
  };
}

}  // namespace Threedim
