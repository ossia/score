#pragma once
#include <isf.hpp>

namespace score::gfx
{
struct isf_input_size_vis
{
  int sz{};
  void operator()(const isf::float_input&) noexcept { sz += 4; }

  void operator()(const isf::long_input&) noexcept { sz += 4; }

  void operator()(const isf::event_input&) noexcept
  {
    sz += 4; // bool
  }

  void operator()(const isf::bool_input&) noexcept
  {
    sz += 4; // bool
  }

  void operator()(const isf::point2d_input&) noexcept
  {
    if(sz % 8 != 0)
      sz += 4;
    sz += 2 * 4;
  }

  void operator()(const isf::point3d_input&) noexcept
  {
    while(sz % 16 != 0)
    {
      sz += 4;
    }
    sz += 3 * 4;
  }

  void operator()(const isf::color_input&) noexcept
  {
    while(sz % 16 != 0)
    {
      sz += 4;
    }
    sz += 4 * 4;
  }

  void operator()(const isf::image_input&) noexcept { }

  void operator()(const isf::audio_input&) noexcept { }
  void operator()(const isf::audioFFT_input&) noexcept { }
  void operator()(const isf::audioHist_input&) noexcept { }

  // CSF-specific input handlers
  void operator()(const isf::storage_input& in) noexcept
  {
    if(in.access.contains("write"))
    {
      (*this)(isf::long_input{});
    }
  }

  void operator()(const isf::texture_input in) noexcept { }

  void operator()(const isf::csf_image_input& in) noexcept
  {
    if(in.access.contains("write"))
    {
      (*this)(isf::point2d_input{});
      (*this)(isf::long_input{});
    }
  }
};
}
