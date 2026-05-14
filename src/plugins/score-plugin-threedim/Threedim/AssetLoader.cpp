#include "AssetLoader.hpp"

#include "FbxParser.hpp"
#include "GltfParser.hpp"
#include "Ply.hpp"
#include "PrimitiveCloud/FormatOverride.hpp"
#include "PrimitiveCloud/PlyParser.hpp"
#include "PrimitiveCloud/SceneFromCloud.hpp"
#include "PrimitiveCloud/SplatBinary.hpp"
#include "PrimitiveCloud/SpzCodec.hpp"
#include "SceneFromMeshes.hpp"
#include "VcgImporters.hpp"

#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>

#include <QFileInfo>
#include <QQuaternion>
#include <QString>

#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace Threedim
{

// =============================================================================
// AssetLoaderRegistry — process-wide parser dispatch table.
//
// Storage is a function-local Meyers singleton so registrations at
// static-init time work without worrying about dynamic-init order across
// translation units. The small-vector-ish layout (O(N) lookup over a
// ~4-entry list) is fine: registrations are one-shot per addon.
// =============================================================================
namespace
{
struct RegistryState
{
  std::mutex mutex;
  std::vector<std::pair<std::string, AssetLoaderRegistry::ParseFn>> entries;
};
RegistryState& registryInstance()
{
  static RegistryState s;
  return s;
}

std::string toLower(std::string_view s)
{
  std::string out;
  out.reserve(s.size());
  for(char c : s)
    out.push_back(char(std::tolower((unsigned char)c)));
  return out;
}
} // namespace

void AssetLoaderRegistry::register_parser(
    std::string_view extension, ParseFn fn)
{
  if(!fn || extension.empty())
    return;
  auto key = toLower(extension);
  auto& r = registryInstance();
  std::lock_guard lock{r.mutex};
  for(auto& e : r.entries)
  {
    if(e.first == key)
    {
      e.second = fn;  // Last writer wins.
      return;
    }
  }
  r.entries.emplace_back(std::move(key), fn);
}

AssetLoaderRegistry::ParseFn
AssetLoaderRegistry::lookup(std::string_view extension_lower) noexcept
{
  if(extension_lower.empty())
    return nullptr;
  auto& r = registryInstance();
  std::lock_guard lock{r.mutex};
  for(auto const& e : r.entries)
    if(e.first == extension_lower)
      return e.second;
  return nullptr;
}

namespace
{

static bool hasSuffixCI(std::string_view path, std::string_view ext) noexcept
{
  if(path.size() < ext.size() + 1)
    return false;
  if(path[path.size() - ext.size() - 1] != '.')
    return false;
  auto a = path.rbegin();
  auto b = ext.rbegin();
  for(; b != ext.rend(); ++a, ++b)
  {
    char x = (char)std::tolower((unsigned char)*a);
    char y = (char)std::tolower((unsigned char)*b);
    if(x != y) return false;
  }
  return true;
}

// Extract the lowercased suffix after the final '.' (no dot). Empty
// on a dotless path. Used to consult AssetLoaderRegistry after the
// built-in dispatch misses.
static std::string extensionLowerCI(std::string_view path)
{
  auto pos = path.find_last_of('.');
  if(pos == std::string_view::npos || pos + 1 >= path.size())
    return {};
  return toLower(path.substr(pos + 1));
}

// Reuse FbxParser / GltfParser's static parsers by constructing a throwaway
// inner instance, invoking the apply-lambda they return, and lifting the
// parsed raw scene_state out. No cross-frame state from the inner loader
// leaks into AssetLoader; its m_raw_state shared_ptr is copied into ours.
//
// Pin the file_type explicitly (halp::text_file_view — the default for
// every loader's halp::file_port<"..."> here). A forwarding-reference
// template parameter deduced from both the data arg and the function
// pointer's by-value parameter produces a deduction conflict
// (FileT& vs FileT), so we skip deduction.
template <typename Loader>
static std::shared_ptr<const ossia::scene_state>
runInnerParser(const halp::text_file_view& data,
               std::function<void(Loader&)> (*parse)(halp::text_file_view))
{
  auto apply = parse(data);
  if(!apply)
    return nullptr;
  Loader inner;
  apply(inner);
  return inner.m_raw_state;
}

} // namespace

std::function<void(AssetLoader&)>
AssetLoader::ins::asset_t::process(file_type tv)
{
  if(tv.filename.empty())
    return {};

  const std::string_view fname{tv.filename};
  std::shared_ptr<const ossia::scene_state> loaded;

  if(hasSuffixCI(fname, "fbx"))
  {
    loaded = runInnerParser<FbxParser>(tv, &FbxParser::ins::fbx_t::process);
  }
  else if(hasSuffixCI(fname, "gltf") || hasSuffixCI(fname, "glb"))
  {
    loaded = runInnerParser<GltfParser>(tv, &GltfParser::ins::gltf_t::process);
  }
  else if(hasSuffixCI(fname, "obj"))
  {
    Threedim::float_vec buf;
    auto meshes = Threedim::ObjFromString(tv.bytes, buf);
    if(!meshes.empty())
    {
      const QString label = QFileInfo(QString::fromStdString(std::string{fname}))
                                .fileName();
      loaded = Threedim::sceneStateFromMeshes(
          std::move(meshes), std::move(buf), label.toStdString());
    }
  }
  else if(hasSuffixCI(fname, "ply"))
  {
    // Sniff the header first: a PLY whose vertex element carries
    // splat-style columns (or no face element) goes through the
    // primitive-cloud path; everything else stays on the existing
    // mesh path. The sniff only reads the textual header, no row data.
    if(Threedim::PrimitiveCloud::ply_is_splat_shaped(fname))
    {
      auto cloud = Threedim::PrimitiveCloud::parse_ply(fname);
      if(cloud)
      {
        const QString label
            = QFileInfo(QString::fromStdString(std::string{fname})).fileName();
        loaded = Threedim::PrimitiveCloud::sceneStateFromCloud(
            std::move(cloud), label.toStdString());
      }
    }
    else
    {
      Threedim::float_vec buf;
      auto meshes = Threedim::PlyFromFile(fname, buf);
      if(!meshes.empty())
      {
        const QString label
            = QFileInfo(QString::fromStdString(std::string{fname})).fileName();
        loaded = Threedim::sceneStateFromMeshes(
            std::move(meshes), std::move(buf), label.toStdString());
      }
    }
  }
  else if(hasSuffixCI(fname, "stl"))
  {
    Threedim::float_vec buf;
    auto meshes = Threedim::StlFromFile(fname, buf);
    if(!meshes.empty())
    {
      const QString label = QFileInfo(QString::fromStdString(std::string{fname}))
                                .fileName();
      loaded = Threedim::sceneStateFromMeshes(
          std::move(meshes), std::move(buf), label.toStdString());
    }
  }
  else if(hasSuffixCI(fname, "off"))
  {
    Threedim::float_vec buf;
    auto meshes = Threedim::OffFromFile(fname, buf);
    if(!meshes.empty())
    {
      const QString label = QFileInfo(QString::fromStdString(std::string{fname}))
                                .fileName();
      loaded = Threedim::sceneStateFromMeshes(
          std::move(meshes), std::move(buf), label.toStdString());
    }
  }
  else if(hasSuffixCI(fname, "splat"))
  {
    // Antimatter15 binary .splat: 32 bytes/primitive, fixed schema.
    auto cloud = Threedim::PrimitiveCloud::parse_splat_binary(tv.bytes);
    if(cloud)
    {
      const QString label
          = QFileInfo(QString::fromStdString(std::string{fname})).fileName();
      loaded = Threedim::PrimitiveCloud::sceneStateFromCloud(
          std::move(cloud), label.toStdString());
    }
  }
  else if(hasSuffixCI(fname, "spz"))
  {
    // Niantic .spz v1-3: gzip-compressed column-grouped 3DGS data.
    // Decoded via the vendored Niantic library (3rdparty/spz),
    // transposed into the canonical 62-float row layout that the
    // 3dgs.classic preset reads. v4 (NGSP-magic + ZSTD) returns
    // nullptr — see 3rdparty/spz/CMakeLists.txt for the rationale.
    auto cloud = Threedim::PrimitiveCloud::parse_spz(tv.bytes);
    if(cloud)
    {
      const QString label
          = QFileInfo(QString::fromStdString(std::string{fname})).fileName();
      loaded = Threedim::PrimitiveCloud::sceneStateFromCloud(
          std::move(cloud), label.toStdString());
    }
  }
  else
  {
    // Built-ins all missed — consult the addon-registered parsers.
    // score-addon-academy registers its USD loader here at module load.
    const std::string ext = extensionLowerCI(fname);
    if(auto fn = AssetLoaderRegistry::lookup(ext))
      loaded = fn(tv);
  }

  if(!loaded)
    return {};

  return [state = std::move(loaded)](AssetLoader& self) mutable {
    self.m_parsed_state = std::move(state);
    self.rebuild_format_state();        // m_parsed → m_overridden
    self.m_cached_xform.valid = false;  // force wrap rebuild
    self.rebuild_wrapped_state();
  };
}

void AssetLoader::rebuild_format_state()
{
  m_cached_format_override = inputs.format_override.value;
  m_overridden_state = Threedim::PrimitiveCloud::applyFormatOverride(
      m_parsed_state, m_cached_format_override);
  // The wrapped state derives from m_overridden_state and must be
  // rebuilt whenever the override changes.
  m_cached_xform.valid = false;
  rebuild_wrapped_state();
}

void AssetLoader::rebuild_wrapped_state()
{
  m_wrapped_state = Threedim::wrapSceneWithTransform(
      m_overridden_state, inputs, m_cached_xform, m_version_counter, m_xform_ref);
}

void AssetLoader::operator()()
{
  if(!m_parsed_state)
  {
    outputs.scene_out.scene.state = nullptr;
    outputs.scene_out.dirty = 0;
    return;
  }

  if(Threedim::transformChanged(inputs, m_cached_xform))
    rebuild_wrapped_state();

  outputs.scene_out.scene.state = m_wrapped_state;
  outputs.scene_out.dirty = ossia::scene_port::dirty_transform;
}

void AssetLoader::init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
{
  if(!raw_transform_slot.valid())
  {
    raw_transform_slot = r.registry().allocate(
        score::gfx::GpuResourceRegistry::Arena::RawTransform,
        sizeof(score::gfx::RawLocalTransform));
    m_xform_ref = r.registry().toOssiaRef(raw_transform_slot);
    // Force the wrapped state to be rebuilt so the emitted
    // scene_transform carries the fresh ref.
    m_cached_xform.valid = false;
  }
  if(raw_transform_slot.valid())
  {
    score::gfx::RawLocalTransform seed{};
    r.registry().updateSlot(res, raw_transform_slot, &seed, sizeof(seed));
  }
}

void AssetLoader::update(
    score::gfx::RenderList& r, QRhiResourceUpdateBatch& res, score::gfx::Edge*)
{
  if(!raw_transform_slot.valid())
    return;

  score::gfx::RawLocalTransform xform{};
  xform.translation[0] = inputs.position.value.x;
  xform.translation[1] = inputs.position.value.y;
  xform.translation[2] = inputs.position.value.z;
  QQuaternion q = QQuaternion::fromEulerAngles(
      inputs.rotation.value.x, inputs.rotation.value.y,
      inputs.rotation.value.z);
  xform.rotation[0] = q.x();
  xform.rotation[1] = q.y();
  xform.rotation[2] = q.z();
  xform.rotation[3] = q.scalar();
  xform.scale[0] = inputs.scale.value.x;
  xform.scale[1] = inputs.scale.value.y;
  xform.scale[2] = inputs.scale.value.z;
  r.registry().updateSlot(res, raw_transform_slot, &xform, sizeof(xform));
}

void AssetLoader::release(score::gfx::RenderList& r)
{
  if(raw_transform_slot.valid())
    r.registry().free(raw_transform_slot);
  m_xform_ref = {};
  // Clear cached scene_state so the next operator()() rebuilds against
  // the post-release registry. Producer-state-drift Option A — see
  // matching comment in Light::release.  m_parsed_state stays valid
  // (parser output, no slot refs); only m_overridden_state and
  // m_wrapped_state embed registry refs and need clearing.
  m_overridden_state.reset();
  m_wrapped_state.reset();
}

} // namespace Threedim
