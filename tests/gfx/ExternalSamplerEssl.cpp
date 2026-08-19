#include <Gfx/Graph/decoders/NV12ExternalOES.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
QByteArray baked()
{
  return QByteArrayLiteral(
      "#version 300 es\n"
      "precision mediump float;\n"
      "uniform highp sampler2D tex;\n"
      "layout(std140) uniform buf { mat4 mvp; } renderer;\n"
      "void main() { }\n");
}
}

TEST_CASE("the external-sampler rewrite rejects source it did not bake", "[gfx][oes]")
{
  const auto ok = score::gfx::toExternalSamplerEssl(baked());
  REQUIRE_FALSE(ok.isEmpty());
  CHECK(ok.contains("GL_OES_EGL_image_external_essl3"));
  CHECK(ok.contains("samplerExternalOES"));
  CHECK_FALSE(ok.contains("sampler2D tex"));

  // No #version at all: there is nothing to anchor the pragma to, so the
  // rewrite must decline rather than emit a shader with the extension spliced
  // in at whatever the last newline happened to be.
  CHECK(score::gfx::toExternalSamplerEssl(
            QByteArrayLiteral("precision mediump float;\nuniform sampler2D t;\n"))
            .isEmpty());
  CHECK(score::gfx::toExternalSamplerEssl(QByteArrayLiteral("")).isEmpty());
  CHECK(score::gfx::toExternalSamplerEssl(QByteArrayLiteral("no newline here")).isEmpty());

  // A #version with no sampler2D is equally un-rewritable.
  CHECK(score::gfx::toExternalSamplerEssl(
            QByteArrayLiteral("#version 300 es\nvoid main() { }\n"))
            .isEmpty());
}
