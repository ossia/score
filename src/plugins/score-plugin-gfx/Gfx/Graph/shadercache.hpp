#pragma once
#include <score/tools/OpenGL.hpp>

#include <QShaderBaker>

#include <unordered_map>

struct ShaderCache
{
public:
  static const std::pair<QShader, QString>& get(const QByteArray& shader, QShader::Stage stage)
  {
    static ShaderCache self;
    if (auto it = self.shaders.find(shader); it != self.shaders.end())
      return it->second;

    self.baker.setSourceString(shader, stage);
    auto res = self.shaders.insert({shader, {self.baker.bake(), self.baker.errorMessage()}});
    return res.first->second;
  }

private:
  score::GLCapabilities m_caps;
  ShaderCache()
  {
    baker.setGeneratedShaders({
                                {QShader::SpirvShader, 100},
                                {QShader::GlslShader, m_caps.shaderVersion}, // or 120 ?
                            #if defined(_WIN32)
                                {QShader::HlslShader, QShaderVersion(50)},
                            #endif
                            #if defined(__APPLE__)
                                {QShader::MslShader, QShaderVersion(12)},
                            #endif
                              });

    baker.setGeneratedShaderVariants({
                                       QShader::Variant{},
                                       QShader::Variant{},
                                   #if defined(_WIN32)
                                       QShader::Variant{},
                                   #endif
                                   #if defined(__APPLE__)
                                       QShader::Variant{},
                                   #endif
                                     });
  }

  QShaderBaker baker;
  std::unordered_map<QByteArray, std::pair<QShader, QString>> shaders;
};

