// =============================================================================
// A writable cubemap reaches Direct3D as a 2D-array VIEW, and the shader the
// author wrote does not change.
//
// THE DEFECT
//
// Direct3D has no writable cube texture type. HLSL's UAV types stop at
// RWTexture2D / RWTexture2DArray / RWTexture3D; RWTextureCube does not exist in
// any shader model. SPIRV-Cross does not paper over that -- its HLSL backend
// aborts the whole translation:
//
//     Shader baking failed: RWTextureCube does not exist in HLSL.
//
// So every ISF/CSF shader declaring a cube storage image (RESOURCES entry with
// TYPE "image_cube" -> `imageCube` in the generated GLSL) failed to bake for
// D3D11 and D3D12, identically at SM 5.0 and at SM 6.1, while OpenGL, Vulkan
// and Metal translated it fine. The message comes out of SPIRV-Cross's
// image_type_hlsl, not out of fxc/dxc, so this is a code-generation failure and
// not a driver one -- which is why it reproduces on this Linux box.
//
// THE FIX, AND WHY IT COSTS THE RUNTIME NOTHING
//
// isf_emit_cube_image_decl (isf.cpp) declares the same texture as an
// `image2DArray` when the target is HLSL, gated on QSHADER_HLSL, which
// QShaderBaker defines per target because ShaderCache::Baker enables
// setPerTargetCompilation(true) -- the same mechanism the prelude's
// QSHADER_SPIRV orientation macros already use.
//
// Qt's D3D backends were ALREADY binding a 2D-array view: a QRhiTexture with
// the CubeMap flag gets its UAV created as D3D11_UAV_DIMENSION_TEXTURE2DARRAY
// (qrhid3d11.cpp, QD3D11Texture::unorderedAccessViewForLevel) and
// D3D12_UAV_DIMENSION_TEXTURE2DARRAY (qrhid3d12.cpp), both with ArraySize 6.
// Only the shader's spelling of that view was untranslatable. Nothing about the
// allocation moves: IsfBindingsBuilder still creates a QRhiTexture::CubeMap, so
// every downstream consumer still binds it as a samplerCube.
//
// THE INVARIANT THIS FILE PINS
//
// The author's API does not change. Same ISF header, same GLSL body, same
// macros, on every backend:
//
//   * the corpus shader on disk still declares TYPE "image_cube" and stores
//     through IMG_STORE_CUBE -- asserted against the bytes on disk, so a fix
//     that "worked" by rewriting the corpus cannot pass;
//   * the parsed descriptor still reports the resource as a cube, which is what
//     IsfBindingsBuilder reads to decide QRhiTexture::CubeMap;
//   * ONE generated source bakes clean for all five backends, taking the cube
//     spelling on four of them and the array spelling only on HLSL;
//   * imageStore's ivec3 (x, y, face) coordinate is the same coordinate on both
//     paths -- a cube face index and an array layer index are the same number
//     in the same slot -- so the D3D store lands on the texel the cube store
//     lands on. Checked in the emitted HLSL: the coordinate goes into the
//     RWTexture2DArray subscript unmodified.
//
// The ONE spelling that genuinely differs between the two GLSL types is
// imageSize(): ivec2 for a cube, ivec3 for a 2D array. It cannot be hidden --
// GLSL has no typedef, an overload cannot differ only in return type, and the
// preprocessor cannot dispatch on an argument's type. The prelude therefore
// carries IMG_SIZE_CUBE(img), which is `imageSize(img).xy` and is ivec2 on
// every backend; the corpus already takes `.xy`. That divergence is asserted
// below rather than left implicit, so it stays a known, documented edge and not
// a surprise.
//
// WHAT THIS FILE CANNOT SEE
//
// It is a translation test, not a render test: no device, no display. That the
// six faces actually come back non-black is asserted by CsfCubeArray.cpp
// (gfx_csf_cube_array), which needs a real backend -- and this box can only run
// OpenGL and Vulkan, i.e. the two backends that never had this defect. The
// Direct3D render leg stays unverified until the Windows sweep runs.
//
//   ctest -R gfx_cube_image_hlsl
// =============================================================================

#include "IsfTestCommon.hpp"

#include <Gfx/Graph/ShaderCache.hpp>

#include <isf.hpp>

#include <QFile>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>

namespace
{
/// The shader version each backend is baked at, as Gfx::Settings::
/// shaderVersionForAPI() hands them out. Spelled out rather than called for the
/// same reason GfxCsfOrientGate.cpp spells it out: the OpenGL answer there
/// builds score::GLCapabilities{}, which wants a context, and nothing here has
/// one.
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
      // fxc caps here and Qt's D3D11 backend looks up exactly {HlslShader, 50}.
      return QShaderVersion(50);
    case score::gfx::D3D12:
      // Whichever of 5.0 / 6.1 Gfx/Settings/Model.cpp picks at runtime (it
      // depends on dxcompiler.dll being present), both are exercised below.
      return QShaderVersion(50);
    default:
      return {};
  }
}

/// Raw bytes of a corpus file, as the author committed them.
QByteArray corpus_bytes(const char* file)
{
  QFile f{score::test::gfx::isf::corpus(file)};
  REQUIRE(f.open(QIODevice::ReadOnly));
  return f.readAll();
}

/// The corpus .cs, parsed the way Gfx::CSF::Model does, with the work-group
/// size filled in the way RenderedCSFNode does -- the baker rejects the
/// ISF_LOCAL_SIZE_* placeholders. Same helper as GfxCsfOrientGate.cpp.
QByteArray compute_source(const QByteArray& raw)
{
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

/// The single baked artefact of a one-target bake.
struct Baked
{
  QString error;
  QShaderKey key;
  QByteArray code;
};

Baked bake(score::gfx::GraphicsApi api, QShaderVersion v, const QByteArray& src)
{
  Baked b;
  const auto& [shader, error]
      = score::gfx::ShaderCache::get(api, v, src, QShader::ComputeStage);
  b.error = error;
  if(!error.isEmpty() || !shader.isValid())
    return b;
  const auto keys = shader.availableShaders();
  if(keys.size() != 1)
  {
    b.error = QStringLiteral("expected exactly one baked variant, got %1")
                  .arg(keys.size());
    return b;
  }
  b.key = keys.front();
  b.code = shader.shader(b.key).shader();
  return b;
}

constexpr const char* kCubeWriter = "csf-cube-image-write.cs";
}

// -----------------------------------------------------------------------------
// 1. One source, five backends, no failures -- the D3D leg being the point.
// -----------------------------------------------------------------------------
TEST_CASE(
    "a writable cubemap bakes for every backend, Direct3D included",
    "[gfx][l1][csf][cube][hlsl]")
{
  // Not platform_backends(): the defect was invisible on the two backends this
  // machine brings up, so a set that shrinks to what the platform runs is
  // exactly the set that cannot see it. Same reasoning as GfxCsfOrientGate.cpp.
  const auto api = GENERATE(
      score::gfx::OpenGL, score::gfx::Vulkan, score::gfx::D3D11, score::gfx::D3D12,
      score::gfx::Metal);
  CAPTURE(score::test::gfx::backend_name(api));

  const QByteArray src = compute_source(corpus_bytes(kCubeWriter));

  const Baked b = bake(api, version_for(api), src);
  INFO("bake error: " << b.error.toStdString());
  REQUIRE(b.error.isEmpty());
  REQUIRE(!b.code.isEmpty());

  switch(b.key.source())
  {
    case QShader::HlslShader:
    {
      // The lowering: Direct3D gets the 2D-array view of the cube, which is the
      // view its UAV already was. RWTextureCube is not a type; if it ever
      // appears here the bake did not fail, it produced something fxc/dxc will.
      INFO("emitted HLSL:\n" << b.code.toStdString());
      CHECK(b.code.contains("RWTexture2DArray"));
      CHECK_FALSE(b.code.contains("RWTextureCube"));
      CHECK_FALSE(b.code.contains("TextureCube"));
      break;
    }
    case QShader::GlslShader:
    {
      // Everyone else keeps the real cube: the lowering is HLSL-only, so a
      // regression that applied it everywhere shows up right here.
      INFO("emitted GLSL:\n" << b.code.toStdString());
      CHECK(b.code.contains("imageCube"));
      CHECK_FALSE(b.code.contains("image2DArray"));
      break;
    }
    case QShader::MslShader:
    {
      // Metal DOES have a writable cube (texturecube<T, access::write>), so it
      // must not be dragged through the D3D lowering either.
      INFO("emitted MSL:\n" << b.code.toStdString());
      CHECK(b.code.contains("texturecube"));
      CHECK_FALSE(b.code.contains("texture2d_array"));
      break;
    }
    default:
      // SPIR-V is binary; that it baked at all is the assertion, and the
      // descriptor check in the next case covers what the engine reads.
      break;
  }
}

// -----------------------------------------------------------------------------
// 2. D3D12 may ask for either shader model. Both must take the lowering.
// -----------------------------------------------------------------------------
TEST_CASE(
    "the cubemap lowering holds at both Direct3D shader models",
    "[gfx][l1][csf][cube][hlsl]")
{
  // Gfx/Settings/Model.cpp probes dxcompiler.dll at runtime and asks for
  // QShaderVersion(61) when it is present, QShaderVersion(50) when it is not.
  // The un-lowered imageCube failed at BOTH, so neither may be left behind.
  const int sm = GENERATE(50, 61);
  CAPTURE(sm);

  const QByteArray src = compute_source(corpus_bytes(kCubeWriter));

  const Baked b = bake(score::gfx::D3D12, QShaderVersion(sm), src);
  INFO("bake error: " << b.error.toStdString());
  REQUIRE(b.error.isEmpty());
  REQUIRE(b.code.contains("RWTexture2DArray"));
}

// -----------------------------------------------------------------------------
// 3. The author's API did not move.
// -----------------------------------------------------------------------------
TEST_CASE(
    "the Direct3D cubemap lowering is invisible in the authored shader",
    "[gfx][l1][csf][cube][hlsl][api]")
{
  const QByteArray raw = corpus_bytes(kCubeWriter);

  // (a) On disk. A "fix" that made D3D pass by rewriting the shader author's
  // source into an image_2d_array with hand-rolled face maths would be exactly
  // the change this is here to forbid, and it would fail here.
  INFO("the corpus shader must still declare a cube resource");
  CHECK(raw.contains("\"TYPE\": \"image_cube\""));
  INFO("...and still store through the cube macro");
  CHECK(raw.contains("IMG_STORE_CUBE(probe, ivec3(xy, face)"));

  // (b) In the parsed descriptor -- what the engine reads to decide the QRhi
  // texture shape. IsfBindingsBuilder turns isCube() into QRhiTexture::CubeMap,
  // so as long as this says cube, the allocated texture is a cube and every
  // downstream consumer still binds a samplerCube.
  isf::parser p{
      std::string(raw.constData(), std::size_t(raw.size())),
      isf::parser::ShaderType::CSF};
  const isf::descriptor desc = p.data();

  int cube_resources = 0;
  for(const auto& inp : desc.inputs)
  {
    if(auto* img = ossia::get_if<isf::csf_image_input>(&inp.data))
    {
      if(img->isCube())
      {
        ++cube_resources;
        CHECK(inp.name == "probe");
        // Cube, not array: the lowering is a per-target VIEW, never a change of
        // the declared resource.
        CHECK_FALSE(img->is_array);
      }
    }
  }
  CHECK(cube_resources == 1);

  // (c) In the generated GLSL: one gated pair, not a rewrite. The cube spelling
  // must still be there for everyone who can take it.
  const QByteArray src = compute_source(raw);
  INFO("generated compute source:\n" << src.toStdString());
  CHECK(src.contains("#if defined(QSHADER_HLSL)"));
  CHECK(src.contains("image2DArray probe;"));
  CHECK(src.contains("imageCube probe;"));

  // (d) The author's body is passed through untouched -- the lowering happens
  // in the declaration and nowhere else.
  CHECK(src.contains("IMG_STORE_CUBE(probe, ivec3(xy, face), vec4(tint, 1.0));"));
}

// -----------------------------------------------------------------------------
// 4. The store lands on the same texel on both paths.
// -----------------------------------------------------------------------------
TEST_CASE(
    "a cube face index and an array layer index are the same coordinate",
    "[gfx][l1][csf][cube][hlsl]")
{
  // This is why the lowering is sound rather than merely compilable: GLSL gives
  // imageStore(imageCube, ivec3(x, y, face), v) and
  // imageStore(image2DArray, ivec3(x, y, layer), v) the same coordinate shape,
  // and QRhi/D3D lays the six faces out as array slices 0..5 in the same order
  // (its cube face order IS the array-slice order -- the UAV it builds over the
  // cube has FirstArraySlice 0, ArraySize 6). So the author's `face` reaches
  // the same texel either way, with no remapping anywhere.
  //
  // The observable: SPIRV-Cross must put the coordinate into the array
  // subscript unmodified. A translation that split, swizzled or offset it would
  // show up as something other than the whole uint3 going in.
  const QByteArray src = compute_source(corpus_bytes(kCubeWriter));
  const Baked b = bake(score::gfx::D3D11, QShaderVersion(50), src);
  INFO("bake error: " << b.error.toStdString());
  REQUIRE(b.error.isEmpty());

  INFO("emitted HLSL:\n" << b.code.toStdString());
  // The author's own (xy, face) goes into the array subscript whole, with no
  // remap, split or reorder around it -- measured, this is exactly what
  // SPIRV-Cross emits for the corpus shader's IMG_STORE_CUBE.
  CHECK(b.code.contains("probe[int3(xy, face)] = "));
}

// -----------------------------------------------------------------------------
// 5. The one divergence, asserted rather than assumed.
// -----------------------------------------------------------------------------
TEST_CASE(
    "imageSize is the one cube spelling the lowering cannot preserve",
    "[gfx][l1][csf][cube][hlsl][api]")
{
  // imageSize() on a cube is ivec2; on a 2D array it is ivec3. Nothing can hide
  // that (no typedef in GLSL, no overload on return type, no type dispatch in
  // the preprocessor), so the contract is: take `.xy`, or use IMG_SIZE_CUBE.
  // The corpus does the former. If a future change makes the raw builtin
  // portable, this case is the thing to revisit -- it is not a wish, it is the
  // documented shape of the API, and it must not drift silently.
  const QByteArray raw = corpus_bytes(kCubeWriter);
  INFO("the corpus cube writer must take the portable .xy of imageSize");
  CHECK(raw.contains("imageSize(probe).xy"));

  // The prelude offers the invariant spelling, so an author never has to know
  // any of this.
  const QByteArray src = compute_source(raw);
  INFO("generated compute source:\n" << src.toStdString());
  CHECK(src.contains("#define IMG_SIZE_CUBE(img) (imageSize(img).xy)"));
}

// -----------------------------------------------------------------------------
// 6. Negative control: what the lowering is actually buying.
// -----------------------------------------------------------------------------
TEST_CASE(
    "an un-lowered imageCube is what Direct3D refuses",
    "[gfx][l1][csf][cube][hlsl]")
{
  // Handled the way ShaderCorpusTargets.cpp handles its known gaps, and for the
  // same reason: this is a property of the SPIRV-Cross inside the host's
  // qtshadertools, not of this tree, and pinning it as a hard expected-failure
  // would turn red on the very Qt that fixed it. So the outcome is CHECKED for
  // its message when it fails and REPORTED when it does not.
  //
  // Measured here on Qt 6.13 and on Qt 6.4.2: it fails, with
  //     Shader baking failed: RWTextureCube does not exist in HLSL.
  static const QByteArray unlowered = R"(#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(binding = 0, rgba8) writeonly uniform imageCube probe;
void main()
{
    ivec2 xy = ivec2(gl_GlobalInvocationID.xy);
    for(int face = 0; face < 6; face++)
        imageStore(probe, ivec3(xy, face), vec4(1.0, 0.5, 0.25, 1.0));
}
)";

  const Baked b = bake(score::gfx::D3D11, QShaderVersion(50), unlowered);
  if(b.error.isEmpty())
  {
    WARN(
        "this Qt's SPIRV-Cross now translates a writable imageCube to HLSL; the "
        "2D-array lowering in isf_emit_cube_image_decl is no longer load-bearing "
        "for this host, but stays required for the shader models that do not");
    SUCCEED();
  }
  else
  {
    INFO("bake error: " << b.error.toStdString());
    CHECK(b.error.contains("RWTextureCube"));
  }

  // Whatever the host's SPIRV-Cross does with the un-lowered form, the LOWERED
  // form must bake -- that is the part this tree controls.
  static const QByteArray lowered = R"(#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
#if defined(QSHADER_HLSL)
layout(binding = 0, rgba8) writeonly uniform image2DArray probe;
#else
layout(binding = 0, rgba8) writeonly uniform imageCube probe;
#endif
void main()
{
    ivec2 xy = ivec2(gl_GlobalInvocationID.xy);
    for(int face = 0; face < 6; face++)
        imageStore(probe, ivec3(xy, face), vec4(1.0, 0.5, 0.25, 1.0));
}
)";
  const Baked ok = bake(score::gfx::D3D11, QShaderVersion(50), lowered);
  INFO("bake error: " << ok.error.toStdString());
  CHECK(ok.error.isEmpty());
}
