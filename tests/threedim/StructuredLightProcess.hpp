#pragma once
#include <Threedim/StructuredLightCalibration.hpp>

namespace slprocess
{
//! The camera input as the host gives it: an RGBA32F frame, or none.
inline void setCamera(Threedim::StructuredLightCalibration& obj, const float* rgba, int w, int h)
{
  auto& t = obj.inputs.camera.texture;
  t.bytes = reinterpret_cast<unsigned char*>(const_cast<float*>(rgba));
  t.width = w;
  t.height = h;
  t.format = halp::custom_variable_texture::RGBA32F;
}

inline void
setPositions(Threedim::StructuredLightCalibration& obj, const float* xyz, int w, int h)
{
  auto& t = obj.inputs.positions.texture;
  t.bytes = const_cast<float*>(xyz);
  t.width = w;
  t.height = h;
}
}
