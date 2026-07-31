#pragma once
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <Gfx/Graph/RenderList.hpp>

namespace Threedim
{

// Sibling to ExtractBuffer2 (name-based buffer extractor) but for
// texture auxiliaries. Reads `inputs.geometry.mesh.auxiliary_textures`
// (populated by the halp/ossia bridge from `ossia::geometry::
// auxiliary_textures` — which ScenePreprocessor fills with skybox,
// irradiance_map, prefiltered_map, brdf_lut, shadow_map_array,
// base_color_array, metal_rough_array, normal_array, emissive_array,
// *_Dyn0..N, and any producer-injected texture) and re-publishes the
// named entry on a standalone gpu_texture_output.
//
// Runtime-detects the texture shape (2D / TextureArray / Cubemap /
// 3D) from QRhiTexture::flags() and stamps it into the output port's
// `kind` field so downstream nodes / shader bindings know how to bind
// (sampler2D / sampler2DArray / samplerCube / sampler3D). Width,
// height, and layer-or-depth count come along from pixelSize() /
// arraySize() / depth().
//
// Primary use case: post-processing shaders that depend on scene
// aux textures without going through the scene cable themselves. E.g.
// the shaderlib/depth set wants `camera` + `camera_prev` UBOs
// (extract via ExtractBuffer2) and sometimes a depth-texture aux
// (this node).
class ExtractTexture
{
public:
  halp_meta(name, "Extract texture (by name)")
  halp_meta(category, "Visuals/Utilities")
  halp_meta(c_name, "extract_texture_by_name")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/extract-texture.html")
  halp_meta(uuid, "4d8f2a6b-7c19-4e05-a3d8-1b6f5e9c2a48")

  struct ins
  {
    struct
    {
      halp_meta(name, "Geometry");
      halp::dynamic_gpu_geometry mesh;
      float transform[16]{};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } geometry;

    struct : halp::lineedit<"Name", "skybox">
    {
      halp_meta(symbol, "name")
    } name;
  } inputs;

  struct
  {
    halp::gpu_texture_output<"Texture"> texture;
  } outputs;

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);
  void operator()() { }

private:
  // Last-known resolved values — used to skip work when nothing changed.
  void* m_lastHandle{};
  std::string m_lastName;
};

}
