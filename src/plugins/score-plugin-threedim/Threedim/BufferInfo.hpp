#pragma once
#include <fmt/format.h>
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <cstdint>
#include <string>

namespace Threedim
{
// Tiny inspector node: takes a halp::gpu_buffer_input and exposes its
// metadata (handle, byte size, byte offset, dirty flag) on regular
// value-output ports plus a single human-readable summary string. Use
// it as a debug breakpoint in any GPU buffer pipeline -- e.g.
// SomeBufferSource -> BufferInfo -> Downstream -- to verify that the
// buffer is actually wired up and that its size matches what the
// downstream expects.
//
// Mirrors the structure of GeometryInfo: pure CPU operator(), no GPU
// init/update/runInitialPasses needed because the framework already
// publishes the gpu_buffer's metadata into our input port each tick.
class BufferInfo
{
public:
  halp_meta(name, "Buffer Info")
  halp_meta(category, "Visuals/Utilities")
  halp_meta(c_name, "buffer_info")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/buffer-info.html")
  halp_meta(uuid, "f1a3d6c8-2b4e-4c5d-8a9f-1e2d3c4b5a60")

  struct
  {
    halp::gpu_buffer_input<"Buffer"> buffer;
  } inputs;

  struct
  {
    // Numeric metadata, exposed individually so it can be patched into
    // other ports (size-driven UBO updates etc.).
    halp::val_port<"Byte size", int64_t> byte_size;
    halp::val_port<"Byte offset", int64_t> byte_offset;
    // Raw native handle as an opaque integer. Useful only for visual
    // identity ("did the upstream rebuild this buffer?"); the value is
    // a QRhiBuffer* on every backend score supports today.
    halp::val_port<"Handle", int64_t> handle;
    halp::val_port<"Changed", bool> changed;
    // One-line, copy-pasteable summary for tooltips / log scraping.
    halp::val_port<"Readable", std::string> readable;
  } outputs;

  void operator()()
  {
    const auto& b = inputs.buffer.buffer;
    outputs.byte_size.value = b.byte_size;
    outputs.byte_offset.value = b.byte_offset;
    outputs.handle.value = reinterpret_cast<std::int64_t>(b.handle);
    outputs.changed.value = b.changed;

    auto& ret = outputs.readable.value;
    ret.clear();
    fmt::format_to(
        std::back_inserter(ret),
        "handle=0x{:x}, byte_size={}, byte_offset={}, changed={}",
        reinterpret_cast<std::uintptr_t>(b.handle), b.byte_size, b.byte_offset,
        b.changed ? "yes" : "no");
  }
};
}
