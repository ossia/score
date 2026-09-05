// =============================================================================
// Which side of the compute storage-image gate each backend lands on, decided
// without a GPU.
//
// ISF_STORE_COORD flips the Y of an imageStore() index on every backend except
// Vulkan. The gate was once written as "OpenGL only", on the reasoning that
// Direct3D and Metal put the framebuffer origin at the top like Vulkan and so
// need nothing. Measured on hardware that is false: a compute storage image
// reaches the delivered picture mirrored on D3D11, D3D12 and Metal exactly as
// it does on OpenGL. Every compute storage image was upside down on those three
// for as long as the wrong gate stood.
//
// gfx_csf_orient_macros asserts the resulting picture and is the test that
// found it -- but only where the backend runs, and the two backends this
// project's CI can run, OpenGL and Vulkan, are precisely the two the wrong gate
// got right. Nothing on a Linux box could tell the two gates apart.
//
// This can, because the gate is resolved by the shader baker and not by the
// driver. QShaderBaker cross-compiles to HLSL and MSL on any host, and it is
// what defines QSHADER_SPIRV / QSHADER_GLSL / QSHADER_HLSL / QSHADER_MSL, so
// baking the same compute shader once per backend resolves each backend's
// branch of the gate here, on this machine, with no device and no display.
//
// The observable is a size query. ISF_STORE_COORD's corrected form is
// `imageSize(img).y - 1 - coord.y`; its identity form has no imageSize in it,
// and the corpus shader is written with no size query of its own -- the
// dispatch is one invocation per texel and the divisor is the literal 63.0. So
// a size query in the baked code means the backend took the corrected branch,
// and its absence means the identity branch, in whichever language the backend
// speaks.
//
// Every backend is checked on every platform: the point is the three this
// machine cannot run.
//
//   ctest -R gfx_csf_orient_gate
// =============================================================================

#include "IsfTestCommon.hpp"

#include <Gfx/Graph/ShaderCache.hpp>

#include <isf.hpp>

#include <QFile>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>
#include <vector>

namespace
{
/// The shader version each backend is baked at, as Gfx::Settings::
/// shaderVersionForAPI() hands them out. Spelled out rather than called
/// because the OpenGL answer there comes from GLCapabilities, which wants a
/// context; nothing in this file does.
QShaderVersion version_for(score::gfx::GraphicsApi api)
{
  switch(api)
  {
    case score::gfx::OpenGL:
      return QShaderVersion(450);
    case score::gfx::Vulkan:
      return QShaderVersion(100);
    case score::gfx::Metal:
      return QShaderVersion(12);
    case score::gfx::D3D11:
    case score::gfx::D3D12:
      return QShaderVersion(50);
    default:
      return {};
  }
}

/// The corpus .cs, parsed the way Gfx::CSF::Model does, with the work-group
/// size filled in the way RenderedCSFNode does -- the baker rejects the
/// ISF_LOCAL_SIZE_* placeholders.
QByteArray compute_source(const QString& path)
{
  QFile f{path};
  REQUIRE(f.open(QIODevice::ReadOnly));
  const QByteArray raw = f.readAll();

  isf::parser p{
      std::string(raw.constData(), std::size_t(raw.size())),
      isf::parser::ShaderType::CSF};
  REQUIRE(p.mode() == isf::descriptor::CSF);

  const isf::descriptor desc = p.data();
  REQUIRE(!desc.csf_passes.empty());
  const auto& ls = desc.csf_passes[0].local_size;

  QString src = QString::fromStdString(p.compute_shader());
  src.replace("ISF_LOCAL_SIZE_X", QString::number(ls[0]));
  src.replace("ISF_LOCAL_SIZE_Y", QString::number(ls[1]));
  src.replace("ISF_LOCAL_SIZE_Z", QString::number(ls[2]));
  return src.toUtf8();
}

/// Does this baked shader ask the storage image for its size?
///
/// SPIR-V is scanned as instructions: after the five-word header every
/// instruction begins with a word holding its length in the high half and its
/// opcode in the low half, and OpImageQuerySize / OpImageQuerySizeLod are 103
/// and 104. The three textual targets are searched for the call SPIRV-Cross
/// emits for imageSize() in each language.
bool queries_image_size(const QShaderKey& key, const QByteArray& code)
{
  switch(key.source())
  {
    case QShader::SpirvShader:
    {
      const auto* words = reinterpret_cast<const quint32*>(code.constData());
      const int n = int(code.size() / sizeof(quint32));
      if(n < 5)
        return false;
      for(int i = 5; i < n;)
      {
        const quint32 len = words[i] >> 16;
        const quint32 op = words[i] & 0xffffu;
        if(len == 0)
          break;
        if(op == 103u || op == 104u)
          return true;
        i += int(len);
      }
      return false;
    }
    case QShader::HlslShader:
      return code.contains("GetDimensions");
    case QShader::MslShader:
      return code.contains("get_width") || code.contains("get_height");
    default:
      return code.contains("imageSize");
  }
}
}

TEST_CASE(
    "the CSF storage-image gate corrects every backend except Vulkan",
    "[gfx][l1][csf][orientation][macros]")
{
  // Not platform_backends(): the wrong gate was invisible on the two backends
  // this platform can run, so a set that shrinks to what the platform runs is
  // exactly the set that cannot see this.
  const auto api = GENERATE(
      score::gfx::OpenGL, score::gfx::Vulkan, score::gfx::D3D11, score::gfx::D3D12,
      score::gfx::Metal);
  CAPTURE(score::test::gfx::backend_name(api));

  const QByteArray src
      = compute_source(score::test::gfx::isf::corpus("csf-orient-store-nosize.cs"));
  INFO("the corpus shader must have no size query of its own");
  REQUIRE(!src.contains("imageSize(outputImage)"));

  const auto& [shader, error]
      = score::gfx::ShaderCache::get(api, version_for(api), src, QShader::ComputeStage);
  INFO("bake error: " << error.toStdString());
  REQUIRE(error.isEmpty());
  REQUIRE(shader.isValid());

  const auto keys = shader.availableShaders();
  REQUIRE(keys.size() == 1);
  const QByteArray code = shader.shader(keys.front()).shader();
  REQUIRE(!code.isEmpty());

  // Vulkan is the one backend whose raw texel index already agrees with the
  // target it is copied into; every other backend needs the Y flipped, and
  // the flip is the only thing that can put a size query in this shader.
  const bool corrected = queries_image_size(keys.front(), code);
  if(api == score::gfx::Vulkan)
    CHECK_FALSE(corrected);
  else
    CHECK(corrected);
}
