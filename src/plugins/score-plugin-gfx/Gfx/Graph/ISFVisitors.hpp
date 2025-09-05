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

  void operator()(const isf::image_input&) noexcept { (*this)(isf::color_input{}); }

  void operator()(const isf::audio_input&) noexcept { (*this)(isf::color_input{}); }
  void operator()(const isf::audioFFT_input&) noexcept { (*this)(isf::color_input{}); }
  void operator()(const isf::audioHist_input&) noexcept { (*this)(isf::color_input{}); }
};

}
