// A pass's WIDTH / HEIGHT expression reads the shader's controls by name
// ($scale). They were bound by walking the inputs one port each, but a written
// storage buffer or image creates no input port, and a flexible written buffer
// or a $USER size creates extra ones: every control after them was bound to a
// neighbouring port's value (shader board F08).
#include <Gfx/Graph/ISFNode.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QString>

TEST_CASE("pass size expressions read the named control", "[gfx][isf][passes][F08]")
{
  // Each resource shifts the port walk differently: none, -1, +1 port.
  const auto resource = GENERATE(
      std::string{},
      std::string{R"_({ "NAME": "results", "TYPE": "storage", "ACCESS": "write_only", "LAYOUT": [ { "NAME": "v", "TYPE": "float[4]" } ] },)_"},
      std::string{R"_({ "NAME": "results", "TYPE": "storage", "ACCESS": "write_only", "LAYOUT": [ { "NAME": "v", "TYPE": "float[]" } ] },)_"});
  CAPTURE(resource);
  const std::string fs = R"_(/*{
  "ISFVSN": "2.0",
  "INPUTS": [
    { "NAME": "first", "TYPE": "float", "DEFAULT": 7.0, "MIN": 0.0, "MAX": 100.0 },
    )_" + resource + R"_(
    { "NAME": "scale", "TYPE": "float", "DEFAULT": 24.0, "MIN": 1.0, "MAX": 100.0 },
    { "NAME": "last", "TYPE": "float", "DEFAULT": 3.0, "MIN": 0.0, "MAX": 100.0 }
  ],
  "PASSES": [ { "TARGET": "buf", "WIDTH": "$scale", "HEIGHT": "$last * 2" }, { } ]
}*/
void main() { gl_FragColor = vec4(0.0); }
)_";
  isf::parser p{"", fs, 450, isf::parser::ShaderType::ISF};
  const auto d = p.data();
  REQUIRE(d.passes.size() == 2);
  score::gfx::ISFNode node{
      d, QString::fromStdString(p.vertex()), QString::fromStdString(p.fragment())};
  const QSize sz = node.computeTextureSize(d.passes[0], QSize{640, 480}, {});
  CHECK(sz.width() == 24);
  CHECK(sz.height() == 6);
}

// Header expressions take libossia's argument order, value first, which is
// not GLSL's: lerp(t, a, b), step(value, edge), smoothstep(value, a, b). It
// is kept so saved scores read the same and documented as such (shader board
// U07); this pins it.
TEST_CASE("pass size expressions take libossia's argument order", "[gfx][isf][passes][U07]")
{
  struct Case
  {
    std::string expr;
    int size;
  };
  const auto c = GENERATE(
      Case{"lerp(0.25, 100, 200)", 125},           // GLSL mix(a, b, t)
      Case{"step(5, 10) * 7 + step(10, 5) * 40", 40}, // GLSL step(edge, x)
      Case{"smoothstep(15, 10, 20) * 64", 32});    // GLSL smoothstep(a, b, x)
  CAPTURE(c.expr);
  const std::string fs = R"_(/*{
  "ISFVSN": "2.0",
  "INPUTS": [],
  "PASSES": [ { "TARGET": "buf", "WIDTH": ")_" + c.expr + R"_(", "HEIGHT": "8" }, { } ]
}*/
void main() { gl_FragColor = vec4(0.0); }
)_";
  isf::parser p{"", fs, 450, isf::parser::ShaderType::ISF};
  const auto d = p.data();
  REQUIRE(d.passes.size() == 2);
  score::gfx::ISFNode node{
      d, QString::fromStdString(p.vertex()), QString::fromStdString(p.fragment())};
  const QSize sz = node.computeTextureSize(d.passes[0], QSize{640, 480}, {});
  CHECK(sz.width() == c.size);
  CHECK(sz.height() == 8);
}
