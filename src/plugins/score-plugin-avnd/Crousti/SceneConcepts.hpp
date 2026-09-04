#pragma once

// Scene port concept — shared between Crousti's port setup (type dispatch,
// port factory) and the GPU upload path.
//
// A halp output struct field is a "scene port" when it carries an
// `ossia::scene_spec scene` field. Scene output travels through the
// existing Gfx::GeometryOutlet / Types::Geometry: a scene is a richer form
// of geometry, same pattern as Process::TexturePort carrying any GPU
// resource.
//
// Once the design proves out, this should be promoted to avendish itself
// (3rdparty/avendish/include/avnd/concepts/gfx.hpp) under a corresponding
// scene concept alongside `geometry_port`.

#include <ossia/dataflow/geometry_port.hpp>

#include <concepts>
#include <cstdint>

namespace oscr
{

template <typename T>
concept scene_port = requires(T t) {
  { t.scene } -> std::convertible_to<const ossia::scene_spec&>;
};

// Dirty-flag lexicon mirrors ossia::scene_port::dirt_flags so shader authors
// can signal fine-grained changes without republishing the whole scene.
// Users set bits on the halp field's `dirty` member; the upload path clears
// them after publishing.
namespace scene_dirt_flags
{
constexpr uint8_t transform   = 0x01;
constexpr uint8_t geometry    = 0x02;
constexpr uint8_t materials   = 0x04;
constexpr uint8_t lights      = 0x08;
constexpr uint8_t animation   = 0x10;
constexpr uint8_t environment = 0x20;
constexpr uint8_t structure   = 0x40;
constexpr uint8_t all         = 0xFF;
}

}
