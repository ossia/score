#pragma once
#include <isf.hpp>

#include <score_plugin_gfx_export.h>

class QString;
namespace Gfx
{
class TextureInlet;

//! A process whose texture inlets are inputs of a shader: what the shader's
//! JSON declares about the sampler of each (shader board U34).
class SCORE_PLUGIN_GFX_EXPORT ShaderSamplerSource
{
public:
  virtual ~ShaderSamplerSource();
  //! Null when the shader declares no sampler for that input.
  virtual const isf::sampler_config*
  shaderSampler(const TextureInlet& inlet) const noexcept = 0;
};

//! The sampler `desc` declares for its input named `name`, or null.
SCORE_PLUGIN_GFX_EXPORT
const isf::sampler_config*
declaredSampler(const isf::descriptor& desc, const QString& name) noexcept;
}
