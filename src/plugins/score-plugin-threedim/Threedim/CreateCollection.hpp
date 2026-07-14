#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Threedim
{

// Authors a named collection (Solaris-style reusable path set) and
// stamps it onto the passthrough scene_spec's collections vector.
//
// Collections are addressable by name anywhere downstream — a consumer
// node that takes a collection name (e.g. a future SceneGraphFilter
// "by collection" mode) resolves the paths at consume-time. This
// decouples "what is the set of things I care about?" from "what am I
// doing to them?" — the classic Solaris LIVRPS composition win.
//
// Multiple CreateCollection nodes can chain: each contributes its own
// named collection to the scene, and downstream consumers can pick any
// of them by name. merge_scenes concatenates collections additively
// across multi-producer merges.
class CreateCollection
{
public:
  halp_meta(name, "Create Collection")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "create_collection")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/create-collection.html")
  halp_meta(uuid, "6c2e9b7a-4d3f-4a1c-8f5e-2b7d9e4c3a1f")

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    // Port-driven rebuild: controls trigger rebuild(); upstream
    // scene_in changes detected in operator()().
    struct : halp::lineedit<"Name", "">
    { void update(CreateCollection& n) { n.rebuild(); } } name;
    struct : halp::val_port<"Paths", std::vector<std::string>>
    { void update(CreateCollection& n) { n.rebuild(); } } paths;
    struct : halp::val_port<"Tags", std::vector<std::string>>
    { void update(CreateCollection& n) { n.rebuild(); } } tags;
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

  std::shared_ptr<const ossia::scene_state> m_cached_out;
  uint8_t m_pending_dirty{0xFF};
  const ossia::scene_state* m_cached_in_state{};
  int64_t m_cached_in_version{-1};
  std::string m_cached_name;
  std::vector<std::string> m_cached_paths;
  std::vector<std::string> m_cached_tags;
  int64_t m_version_counter{0};
};

}
