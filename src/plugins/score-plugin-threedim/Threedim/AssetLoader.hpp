#pragma once
#include <Threedim/TinyObj.hpp>
#include <Threedim/TransformHelper.hpp>
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>

#include <score_plugin_threedim_export.h>

#include <memory>
#include <string_view>

class QRhiResourceUpdateBatch;

namespace score::gfx
{
class RenderList;
struct Edge;
}

namespace Threedim
{

// External scene-file parser registry. An addon shipping a format-specific
// parser registers itself here so AssetLoader can dispatch to it without a
// link-time dependency from score-plugin-threedim.
//
// The callback takes the same halp::text_file_view the built-in glTF and FBX
// parsers receive and returns a populated ossia::scene_state, or a null
// shared_ptr on failure or unhandled input. AssetLoader wraps the state with the
// Position / Rotation / Scale controls exactly as for the built-ins.
//
// Extensions match case-insensitively on the suffix after the final '.'. A
// duplicate registration replaces the prior one. Calls are thread-safe.
class SCORE_PLUGIN_THREEDIM_EXPORT AssetLoaderRegistry
{
public:
  using ParseFn = std::shared_ptr<const ossia::scene_state> (*)(
      const halp::text_file_view&);

  // Register a parser for an extension (without the dot). Safe at
  // static-init time — the underlying storage is a function-local
  // Meyers singleton.
  static void register_parser(std::string_view extension, ParseFn fn);

  // Lookup by lowercased extension. Returns nullptr if no match.
  static ParseFn lookup(std::string_view extension_lower) noexcept;
};

// Unified 3D asset loader. Accepts .fbx / .gltf / .glb / .obj / .ply / .stl /
// .off natively, plus .usd / .usda / .usdc / .usdz when score-addon-academy is
// loaded and has registered its UsdParser.
//
// Dispatch is by file extension:
//   .fbx                 ufbx           (FbxParser's static parser)
//   .gltf / .glb         fastgltf       (GltfParser's static parser)
//   .obj                 tinyobjloader  + sceneStateFromMeshes
//   .ply                 miniply        + sceneStateFromMeshes
//   .stl / .off          vcglib         + sceneStateFromMeshes
//   .usd*                OpenUSD        (academy UsdParser, optional)
//   others               AssetLoaderRegistry::lookup(ext)
//
// Position / Rotation / Scale controls wrap the loaded scene at a single root
// TRS through TransformHelper::wrapSceneWithTransform.
//
// The geometry-only formats produce a scene with one scene_node per mesh part,
// each holding a mesh_component over a single shared CPU buffer; FBX and glTF
// retain their scene hierarchy of lights, cameras, materials, skeletons and
// animations.
class AssetLoader
{
public:
  halp_meta(name, "Asset Loader")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "asset_loader")
  halp_meta(authors, "ossia team, ufbx / fastgltf / tinyobj / miniply / vcglib")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/asset-loader.html")
  halp_meta(uuid, "2f6a8c41-7d93-4e5b-b1c8-4e3f9a7d2c5b")

  struct ins
  {
    struct asset_t : halp::file_port<"Asset file">
    {
      halp_meta(
          extensions,
          "3D assets (*.fbx *.gltf *.glb *.obj *.ply *.stl *.off "
          "*.splat *.spz "
          "*.usd *.usda *.usdc *.usdz)");
      static std::function<void(AssetLoader&)> process(file_type data);
    } asset;

    PositionControl position;
    RotationControl rotation;
    ScaleControl    scale;

    // Stamps every primitive_cloud_component emitted by this asset
    // with `format_id = value` when non-empty. Empty falls back to the
    // parser's autodetection (PLY column sniffing, .splat / .spz
    // hardcoded). Used to route unrecognised PLY columns or addon-
    // produced files through a FlattenedSceneFilterNode in mode 12.
    struct format_override_t : halp::lineedit<"Format override (auto if empty)", "">
    {
      void update(AssetLoader& n) { n.rebuild_format_state(); }
    } format_override;
  } inputs;

  struct outs
  {
    struct
    {
      halp_meta(name, "Scene");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_out;
  } outputs;

  void operator()();

  // Render-thread hooks. init() claims a RawTransform slot for the
  // single root wrapping xform this node emits (TransformHelper's
  // scene-wrapping transform). update() uploads the current TRS.
  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);

  // Raw scene as parsed from the file — stable as long as the file
  // doesn't change. The pipeline is:
  //   m_parsed_state         (parser output, never mutated)
  //   ↓ applyFormatOverride(format_override.value)
  //   m_overridden_state     (format_id rewrites applied, or = parsed)
  //   ↓ wrapSceneWithTransform(position/rotation/scale)
  //   m_wrapped_state        (final, published downstream)
  std::shared_ptr<const ossia::scene_state> m_parsed_state;
  std::shared_ptr<const ossia::scene_state> m_overridden_state;
  std::shared_ptr<const ossia::scene_state> m_wrapped_state;
  std::string m_cached_format_override;
  CachedTRS m_cached_xform;
  int64_t m_version_counter{0};

  // Re-runs applyFormatOverride from the parsed state. Triggered by the
  // lineedit's update() callback when the user edits the override
  // field; also called once after parsing.
  void rebuild_format_state();

  score::gfx::GpuResourceRegistry::Slot raw_transform_slot;
  ossia::gpu_slot_ref m_xform_ref{};

private:
  void rebuild_wrapped_state();
};

}
