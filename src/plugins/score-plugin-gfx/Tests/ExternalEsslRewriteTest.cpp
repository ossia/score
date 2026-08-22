// Unit tests for toExternalSamplerEssl (Gfx/Graph/decoders/NV12ExternalOES.cpp):
// the two surgical edits that turn baked GLSL into the external-sampler
// variant a dma-buf NV12 frame can be sampled through.
//
// `samplerExternalOES` cannot be baked -- glslang rejects the type when
// targeting SPIR-V -- so the shader is baked with `sampler2D` and only the GLSL
// text handed to the GL backend is rewritten. The first attempt at that
// reconstructed the declarations by hand and produced source no driver would
// compile: score's uniform blocks end `} renderer;`, not `};`, so slicing to
// the first `};` cut the block name off and the compile failed at token
// "renderer". Hence the assertions below are about what must survive the edit,
// not only about what it inserts.
//
// The TU is compiled into the test because score_plugin_gfx is built with
// hidden visibility and this function is not exported -- the same reason
// tests/gfx/CMakeLists.txt builds its engine glue lib. src/ is not modified.

#include <Gfx/Graph/decoders/NV12ExternalOES.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using score::gfx::toExternalSamplerEssl;

namespace
{
// A faithful sample of what QShaderBaker emits for a score video shader: the
// uniform blocks carry instance names, and the sampler line carries whatever
// precision and binding qualifiers the reflection produced.
const QByteArray kBaked = QByteArrayLiteral(
    "#version 300 es\n"
    "precision mediump float;\n"
    "layout(std140, binding = 0) uniform buf {\n"
    "    mat4 clipSpaceCorrMatrix;\n"
    "    vec2 texcoordAdjust;\n"
    "} renderer;\n"
    "layout(std140, binding = 2) uniform buf1 {\n"
    "    vec2 scale;\n"
    "} material;\n"
    "layout(binding = 3) uniform highp sampler2D tex;\n"
    "in vec2 v_texcoord;\n"
    "layout(location = 0) out vec4 fragColor;\n"
    "void main()\n"
    "{\n"
    "    fragColor = texture(tex, v_texcoord);\n"
    "}\n");

const QByteArray kSamplerLine
    = QByteArrayLiteral("layout(binding = 3) uniform highp sampler2D tex;");
const QByteArray kPragma
    = QByteArrayLiteral("#extension GL_OES_EGL_image_external_essl3 : require");

QByteArray line(const QByteArray& src, int n)
{
  return src.split('\n').value(n);
}

int count(const QByteArray& src, char c)
{
  return int(std::count(src.begin(), src.end(), c));
}
} // namespace

// The pragma has to be the second line: GLSL requires #version first and
// #extension before any declaration.
TEST_CASE("the extension pragma lands immediately after #version")
{
  const auto out = toExternalSamplerEssl(kBaked);
  REQUIRE_FALSE(out.isEmpty());
  CHECK(line(out, 0) == "#version 300 es");
  CHECK(line(out, 1) == kPragma);
}

TEST_CASE("the whole sampler declaration is replaced, qualifiers and all")
{
  const auto out = toExternalSamplerEssl(kBaked);
  REQUIRE_FALSE(out.isEmpty());

  CHECK(out.contains("uniform samplerExternalOES tex;"));
  CHECK_FALSE(out.contains("sampler2D"));
  // An external sampler takes no precision qualifier and no binding from the
  // reflection; a leftover fragment of the old line is a syntax error.
  CHECK_FALSE(out.contains("highp"));
  CHECK_FALSE(out.contains("binding = 3"));
  // ...and the replacement is the entire line, indentation included.
  CHECK(line(out, 10) == "uniform samplerExternalOES tex;");
}

// The exact regression. A rewrite that reconstructs the declarations instead of
// editing them loses the block instance names, and the driver rejects the
// result at the token that follows the closing brace.
TEST_CASE("the named uniform blocks survive intact")
{
  const auto out = toExternalSamplerEssl(kBaked);
  REQUIRE_FALSE(out.isEmpty());

  CHECK(out.contains("} renderer;"));
  CHECK(out.contains("} material;"));
  CHECK(out.contains("uniform buf {"));
  CHECK(out.contains("uniform buf1 {"));
  CHECK(count(out, '{') == count(out, '}'));
  CHECK(count(out, '{') == count(kBaked, '{'));
}

// Nothing else may move. Undoing the two edits has to give the input back byte
// for byte, which a reconstruction cannot manage.
TEST_CASE("the rewrite changes exactly two lines")
{
  auto out = toExternalSamplerEssl(kBaked);
  REQUIRE_FALSE(out.isEmpty());

  out.remove(out.indexOf(kPragma), kPragma.size() + 1);
  out.replace("uniform samplerExternalOES tex;", kSamplerLine);
  CHECK(out == kBaked);
}

// The caller's contract: an empty return means "leave the baked shader alone
// and warn", which is what makes the rung decline instead of rendering nothing.
TEST_CASE("an input that cannot be rewritten returns empty")
{
  CHECK(toExternalSamplerEssl(QByteArray{}).isEmpty());
  CHECK(toExternalSamplerEssl("#version 300 es\nvoid main(){}\n").isEmpty());
  CHECK(toExternalSamplerEssl("uniform sampler2D tex;").isEmpty());
}

// Feeding the output back in finds no sampler2D and refuses, rather than
// inserting a second pragma and mangling the declaration further.
TEST_CASE("a second rewrite is refused rather than applied")
{
  const auto once = toExternalSamplerEssl(kBaked);
  REQUIRE_FALSE(once.isEmpty());
  CHECK(toExternalSamplerEssl(once).isEmpty());
}
