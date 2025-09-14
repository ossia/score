#pragma once

#include <ossia/detail/pod_vector.hpp>

#include <boost/container/vector.hpp>

#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <algorithm>

namespace Threedim
{
struct custom_texture
{
  using uninitialized_bytes = boost::container::vector<unsigned char>;
  unsigned char* bytes;
  int width;
  int height;
  enum : uint8_t
  {
    RGBA8,
    BGRA8,
    R8,
    RG8,
    R16,
    RG16,
    RED_OR_ALPHA8,

    RGBA16F,
    RGBA32F,
    R16F,
    R32F,

    R8UI,
    R32UI,
    RG32UI,
    RGBA32UI,
  } format
      = RGBA8;
  bool changed;

  int bytes_per_pixel() const noexcept
  {
    switch(format)
    {
      case RGBA8:
      case BGRA8:
        return 4 * 1;
      case R8:
        return 1 * 1;
      case RG8:
        return 2 * 1;
      case R16:
        return 1 * 2;
      case RG16:
        return 2 * 2;
      case RED_OR_ALPHA8:
        return 1 * 1;
      case RGBA16F:
        return 4 * 2;
      case RGBA32F:
        return 4 * 4;
      case R16F:
        return 1 * 2;
      case R32F:
        return 1 * 4;
      case R8UI:
        return 1 * 1;
      case R32UI:
        return 1 * 4;
      case RG32UI:
        return 2 * 4;
      case RGBA32UI:
        return 4 * 4;
      default:
        return 1;
    }
  }
  auto bytesize() const noexcept { return bytes_per_pixel() * width * height; }
  /* FIXME the allocation should not be managed by the plug-in */
  auto allocate(int width, int height)
  {
    using namespace boost::container;
    return uninitialized_bytes(bytesize(), default_init);
  }

  void update(unsigned char* data, int w, int h) noexcept
  {
    bytes = data;
    width = w;
    height = h;
    changed = true;
  }
};
class ArrayToTexture
{
public:
  halp_meta(name, "Array to texture")
  halp_meta(category, "Visuals/Textures")
  halp_meta(c_name, "array_to_texture")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/array-to-texture.html")
  halp_meta(uuid, "bb5dc513-3430-4671-8c74-2bba78e53709")

  struct ins
  {
    struct : halp::val_port<"Input", std::vector<float>>
    {
      void update(ArrayToTexture& self)
      {
        auto sz = self.inputs.size.value;
        self.outputs.main.create(sz.x, sz.y);
        std::size_t to_copy = self.outputs.main.texture.bytesize() / 4;
        to_copy = std::min(to_copy, value.size());
        std::copy_n(value.data(), to_copy, (float*)self.outputs.main.texture.bytes);
        self.outputs.main.upload();
      }
    } in;
    halp::xy_spinboxes_i32<"Size"> size;
  } inputs;

  struct
  {
    halp::texture_output<"Output", custom_texture> main;
  } outputs;
};

}
