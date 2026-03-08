#pragma once
#include <QString>

#include <isf.hpp>

namespace ClipLauncher::Execution
{

// Generates a Video Mixer ISF shader at runtime with exactly N inputs
struct VideoMixerShader
{
  // The constant blend mode GLSL function implementations
  static const QString& blendFunctions();

  // Generate a complete ISF fragment shader for N inputs
  static QString generate(int numInputs);

  // Parse the generated shader to get the ISF descriptor
  static isf::descriptor parseDescriptor(int numInputs);
};

} // namespace ClipLauncher::Execution
