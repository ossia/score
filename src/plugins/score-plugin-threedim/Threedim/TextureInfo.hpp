#pragma once
#include <fmt/format.h>
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace Threedim
{
// Tiny inspector node: takes a halp::gpu_texture_input -- a zero-copy
// reference to the upstream's GPU texture -- and exposes its metadata
// (width, height, format, native handle) on regular value-output ports
// plus a single human-readable summary string.
//
// Wiring: the avnd bridge (texture_inputs_storage) resolves the inlet every
// frame. Unwired, the handle is null and the size 0x0. With one cable and no
// size or format set on the inlet, a texture the upstream publishes
// (textureForOutput) is reported; otherwise the inlet's render target is.
class TextureInfo
{
public:
  halp_meta(name, "Texture Info")
  halp_meta(category, "Visuals/Utilities")
  halp_meta(c_name, "texture_info")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/texture-info.html")
  halp_meta(uuid, "5bd9c8e2-7f1a-4e3b-9c0d-2a4b6f8e1d72")

  struct
  {
    halp::gpu_texture_input<"Texture"> texture;
  } inputs;

  struct
  {
    halp::val_port<"Width", int> width;
    halp::val_port<"Height", int> height;
    halp::val_port<"Format", std::string> format;
    // Raw native handle as an opaque integer (a QRhiTexture* on every
    // backend score supports today). Useful only for visual identity
    // ("did the upstream rebuild this texture?").
    halp::val_port<"Handle", int64_t> handle;
    halp::val_port<"Readable", std::string> readable;
  } outputs;

  static std::string_view format_name(halp::gpu_texture::format_t f) noexcept
  {
    using F = halp::gpu_texture;
    switch(f)
    {
      case F::RGBA8:
        return "RGBA8";
      case F::RGBA16F:
        return "RGBA16F";
      case F::RGBA32F:
        return "RGBA32F";
      case F::R8:
        return "R8";
      case F::R16:
        return "R16";
      case F::R16F:
        return "R16F";
      case F::R32F:
        return "R32F";
      default:
        return "unknown";
    }
  }

  void operator()()
  {
    const auto& t = inputs.texture.texture;
    const auto fmt_name = format_name(t.format);

    outputs.width.value = t.width;
    outputs.height.value = t.height;
    outputs.format.value = std::string{fmt_name};
    outputs.handle.value = reinterpret_cast<std::int64_t>(t.handle);

    auto& ret = outputs.readable.value;
    ret.clear();
    fmt::format_to(
        std::back_inserter(ret), "{}x{} {} (handle=0x{:x})", t.width, t.height,
        fmt_name, reinterpret_cast<std::uintptr_t>(t.handle));
  }
};
}
