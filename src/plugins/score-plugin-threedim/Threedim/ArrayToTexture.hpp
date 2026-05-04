#pragma once

#include <ossia/detail/pod_vector.hpp>

#include <boost/container/vector.hpp>

#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <algorithm>
#include <cstring>

#if (!defined(__linux__) && !defined(_MSC_VER))
#define SCORE_LIBC_HAS_FLOAT16 1
#elif (defined(__linux__) && defined(__GLIBC__))
#if __GLIBC_PREREQ(2, 40)
#define SCORE_LIBC_HAS_FLOAT16 1
#endif
#endif

// when you enter the least obvious syntax competition and your opponent is clang builtins
#if defined(__is_identifier) && !defined(_MSC_VER)
#if (!__is_identifier(_Float16))
#define SCORE_COMPILER_HAS_FLOAT16 1
#endif
#endif

namespace Threedim
{
class ArrayToTexture
{
public:
  halp_meta(name, "Array to texture")
  halp_meta(category, "Visuals/Utilities")
  halp_meta(c_name, "array_to_texture")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/array-to-texture.html")
  halp_meta(uuid, "bb5dc513-3430-4671-8c74-2bba78e53709")

  struct ins
  {
    struct : halp::val_port<"Input", std::vector<float>>
    {
      void update(ArrayToTexture& self) { self.recreate(); }
    } in;
    struct : halp::xy_spinboxes_i32<"Size">
    {
      void update(ArrayToTexture& self) { self.recreate(); }
    } size;
    struct : halp::enum_t<halp::custom_texture::texture_format, "Format">
    {
      void update(ArrayToTexture& self) { self.recreate(); }
    } format;
  } inputs;

  struct
  {
    halp::texture_output<"Output", halp::custom_texture> main;
  } outputs;

  void recreate()
  {
    const auto format = inputs.format;
    const auto sz = inputs.size.value;
    outputs.main.texture.request_format = format;
    outputs.main.create(sz.x, sz.y);
    std::size_t to_copy = sz.x * sz.y * outputs.main.texture.components(format);
    const auto& value = inputs.in.value;
    to_copy = std::min(to_copy, value.size());

    auto* out = outputs.main.texture.bytes;
    using enum halp::custom_texture::texture_format;
    switch(format)
    {
      case RGBA8:
      case BGRA8:
      case R8:
      case RG8:
      case RED_OR_ALPHA8:
      case R8UI:
        std::copy_n(value.data(), to_copy, (uint8_t*)out);
        break;

      case R16:
      case RG16:
        std::copy_n(value.data(), to_copy, (uint16_t*)out);
        break;

      case R32UI:
      case RG32UI:
      case RGBA32UI:
        std::copy_n(value.data(), to_copy, (uint32_t*)out);
        break;

      case R32F:
      case RGBA32F:
        std::copy_n(value.data(), to_copy, (float*)out);
        break;

      case R16F:
      case RGBA16F:

#if (SCORE_LIBC_HAS_FLOAT16 && SCORE_COMPILER_HAS_FLOAT16)
        std::copy_n(value.data(), to_copy, (_Float16*)out);
#else
      {
        // No native _Float16: software IEEE 754 binary16 conversion
        // (round-to-nearest-even) so the upload is still meaningful.
        auto* dst = (uint16_t*)out;
        for(std::size_t i = 0; i < to_copy; i++)
        {
          const float f = value.data()[i];
          uint32_t x;
          std::memcpy(&x, &f, 4);
          const uint32_t sign = (x >> 16) & 0x8000;
          x &= 0x7FFFFFFF;
          if(x >= 0x47800000) // overflow / inf / NaN -> inf (or NaN)
            dst[i] = sign | 0x7C00 | (x > 0x7F800000 ? 0x200 : 0);
          else if(x < 0x38800000) // too small for a normal half
          {
            const uint32_t shift = 126 - (x >> 23); // >= 14
            uint32_t half = 0;
            if(shift <= 24)
            {
              const uint32_t mant = (x & 0x7FFFFF) | 0x800000;
              half = mant >> shift;
              half += ((mant >> (shift - 1)) & 1)
                      && ((mant & ((1u << (shift - 1)) - 1)) || (half & 1));
            }
            dst[i] = sign | half;
          }
          else
          {
            uint32_t half = ((x - 0x38000000) >> 13);
            half += ((x >> 12) & 1) && ((x & 0xFFF) || (half & 1));
            dst[i] = sign | half;
          }
        }
      }
#endif
        break;

      default:
        break;
    }
    outputs.main.upload();
  }
};

}
