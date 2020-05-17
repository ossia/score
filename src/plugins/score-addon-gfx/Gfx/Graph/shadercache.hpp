#pragma once
#include <QShaderBaker>

struct ShaderCache
{
public:
  static const std::pair<QShader, QString>& get(const QByteArray& shader, QShader::Stage stage)
  {
    static ShaderCache self;
    if(auto it = self.shaders.find(shader); it != self.shaders.end())
      return it->second;

    self.baker.setSourceString(shader, stage);
    self.baker.setGeneratedShaders({
               {QShader::SpirvShader, 100},
               {QShader::GlslShader, 120}, // Only GLSL version supported by RHI right now.
               {QShader::HlslShader, QShaderVersion(50)},
               {QShader::MslShader, QShaderVersion(12)},
    });

    auto res = self.shaders.insert({shader, {self.baker.bake(), self.baker.errorMessage()}});
    return res.first->second;
  }

private:
  QShaderBaker baker;
  std::unordered_map<QByteArray, std::pair<QShader, QString>> shaders;
};
