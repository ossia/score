#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace Threedim
{

// Mid-pipeline format-id stamp. Walks every primitive_cloud_component
// reachable from the upstream scene_state and shallow-clones it with
// `format_id = inputs.format_id.value`. Heavy fields (raw_data,
// extra_buffers, bounds) are shared via shared_ptr — no GPU upload
// duplicates.
//
// Wiring:
//   ThirdPartyProducer → TagAs(format_id="my-custom-format")
//                       → ScenePreprocessor
//                       → FlattenedSceneFilter(mode=12, match="my-custom-format")
//                       → CustomDecode → CustomDraw → Window
//
// Use this when the upstream producer can't be modified (third-party
// node, legacy plugin) but the cloud needs to flow through a
// FlattenedSceneFilter in mode 12 (format_id == match_str). Empty
// `format_id` is passthrough — no rewrite, original scene_state
// forwarded as-is.
class TagAs
{
public:
  halp_meta(name, "Tag As Format")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "tag_as_format")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/tag-as-format.html")
  halp_meta(uuid, "8e3d7c2a-5f91-4b6c-a8e2-1d9f4c7b3e5a")

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    struct : halp::lineedit<"Format ID", "">
    { void update(TagAs& n) { n.rebuild(); } } format_id;
  } inputs;

  struct outs
  {
    struct
    {
      halp_meta(name, "Scene Out");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_out;
  } outputs;

  void rebuild();
  void operator()();

  // Cached output kept stable while inputs are unchanged — preserves
  // ScenePreprocessor's fingerprint fast-path.
  std::shared_ptr<const ossia::scene_state> m_cached_out;
  uint8_t m_pending_dirty{0xFF};
  const ossia::scene_state* m_cached_in_state{};
  int64_t m_cached_in_version{-1};
  std::string m_cached_format_id;
};

}
