#pragma once

#include <score_plugin_gfx_export.h>

#include <string>
#include <string_view>
#include <vector>

namespace Gfx
{

// Process-wide registry of primitive_cloud formats. Format bundles
// (built-in or addon-provided) call `register_format` at module init
// to advertise themselves to UI surfaces (AssetLoader's "Format
// override" combobox, FlattenedSceneFilter's "Format ID" picker, the
// future TagAs combobox). Engine code never reads this — the runtime
// pipeline is hash-driven and stays format-agnostic.
//
// The contract: a format bundle is identified by a stable `format_id`
// string ("3dgs.classic", "voxels.octree.v1", "2dgs.surfel"), and
// produces clouds whose `primitive_cloud_component::format_id` matches.
// `display_name` and `description` are user-facing only.
//
// Thread-safe (Meyers singleton + mutex). Last-writer-wins on duplicate
// `format_id` so an addon can override a built-in registration when it
// ships a richer description.
class SCORE_PLUGIN_GFX_EXPORT FormatRegistry
{
public:
  struct Entry
  {
    std::string format_id;       // canonical, stable hash key
    std::string display_name;    // for combobox + tooltip
    std::string description;     // optional longer prose
  };

  static void register_format(Entry e);
  static std::vector<Entry> all();
  static const Entry* find(std::string_view format_id) noexcept;
};

} // namespace Gfx
