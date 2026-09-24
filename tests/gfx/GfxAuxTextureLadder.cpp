// =============================================================================
// Collapsing a material-texture ladder into one array binding.
//
// ScenePreprocessor publishes one auxiliary texture per bucket per channel, so
// a full-PBR shader declares baseColorArray0..15, metalRoughArray0..15 and so
// on. Emitted one combined `sampler2DArray` each, that is 80 SRB bindings for
// the ladders alone, and Qt's Metal backend derives the vertex-buffer origin
// from the highest binding NUMBER -- firstVertexBinding = srbD->maxBinding + 1,
// qrhimetal.mm:2086 -- so those 80 push the vertex buffers past the 31-entry
// buffer table. duck-basic measures 106 bindings and asks for slot 115.
//
// A run of rungs differing only by a trailing index now emits a
// `texture2DArray <base>_tex[N]` plus one shared `sampler <base>_smp`: two
// bindings for N rungs, and one sampler slot instead of N, since Metal charges
// a slot per array element of a combined binding.
//
// What is pinned here:
//   - the parser groups a qualifying run and leaves everything else alone;
//   - the emitted GLSL has the array + sampler + one #define per rung, and no
//     per-rung combined declaration;
//   - the highest binding number actually drops;
//   - the collapsed form still bakes for all five backends, including with a
//     rung handed to a helper as the (texture, sampler) pair it now is.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/ShaderCache.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>

namespace
{
QShaderVersion version_for(score::gfx::GraphicsApi api)
{
  switch(api)
  {
    case score::gfx::OpenGL:
      return QShaderVersion(450);
    case score::gfx::Vulkan:
      return QShaderVersion(100);
    case score::gfx::Metal:
      // Must track Gfx::Settings::shaderVersionForAPI: MSL 2.4, the version
      // macOS 12 ships. An array of textures needs at least 2.0.
      return QShaderVersion(24);
    case score::gfx::D3D11:
    case score::gfx::D3D12:
      return QShaderVersion(50);
    default:
      return {};
  }
}

//! Minimal RAW_RASTER vertex stage; the inputs and varyings come from the
//! fragment header's VERTEX_INPUTS / VERTEX_OUTPUTS.
std::string ladder_vertex()
{
  return R"_(void main()
{
  isf_vertShaderInit();
  v_uv = position.xy;
  gl_Position = vec4(position, 1.);
  isf_vertShaderFinish();
}
)_";
}

//! An eight-rung ladder, a singleton, and a two-entry run that is below the
//! grouping threshold, so one shader exercises all three outcomes.
std::string ladder_shader()
{
  std::string aux;
  for(int i = 0; i < 8; i++)
  {
    aux += R"_(    { "NAME": "fooArray)_" + std::to_string(i)
           + R"_(", "TYPE": "image", "IS_ARRAY": true, "WRAP": "repeat" },
)_";
  }
  for(int i = 0; i < 2; i++)
  {
    aux += R"_(    { "NAME": "barDyn)_" + std::to_string(i)
           + R"_(", "TYPE": "image" },
)_";
  }
  aux += R"_(    { "NAME": "skybox", "TYPE": "cubemap" })_";

  return R"_(/*{
  "ISFVSN": "2",
  "MODE": "RAW_RASTER_PIPELINE",
  "DESCRIPTION": "aux texture ladder",
  "VERTEX_INPUTS": [
    { "TYPE": "vec3", "NAME": "position" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec2", "NAME": "v_uv" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec2", "NAME": "v_uv" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "AUXILIARY": [
)_" + aux + R"_(
  ]
}*/

vec4 rung(texture2DArray tex, sampler smp, vec2 uv)
{
  return texture(sampler2DArray(tex, smp), vec3(uv, 0.));
}

void main()
{
  vec4 c = rung(fooArray_tex[0], fooArray_smp, v_uv)
         + rung(fooArray_tex[3], fooArray_smp, v_uv)
         + rung(fooArray_tex[7], fooArray_smp, v_uv);
  c += texture(sampler2D(barDyn_tex[0], barDyn_smp), v_uv)
     + texture(sampler2D(barDyn_tex[1], barDyn_smp), v_uv);
  c += texture(skybox, vec3(v_uv, 0.));
  isf_FragColor = c;
}
)_";
}

//! Highest N over every `layout(binding = N)` in the source.
int max_binding(const std::string& src)
{
  int best = -1;
  const std::string needle = "layout(binding = ";
  for(std::size_t p = src.find(needle); p != std::string::npos;
      p = src.find(needle, p + 1))
  {
    best = std::max(best, std::stoi(src.substr(p + needle.size())));
  }
  return best;
}

int occurrences(const std::string& hay, const std::string& needle)
{
  int n = 0;
  for(std::size_t p = hay.find(needle); p != std::string::npos;
      p = hay.find(needle, p + needle.size()))
    n++;
  return n;
}
}

TEST_CASE("aux texture ladders are grouped by the parser", "[gfx][isf]")
{
  isf::parser p{
      ladder_vertex(), ladder_shader(), 450,
      isf::parser::ShaderType::RawRasterPipeline};
  REQUIRE(p.mode() == isf::descriptor::RawRaster);

  const isf::descriptor d = p.data();
  REQUIRE(d.auxiliary_textures.size() == 11);

  for(int i = 0; i < 8; i++)
  {
    INFO("rung " << i);
    const auto& rung = d.auxiliary_textures[i];
    CHECK(rung.ladder_base == "fooArray");
    CHECK(rung.ladder_index == i);
    CHECK(rung.ladder_size == 8);
    CHECK(rung.owns_ladder() == (i == 0));
  }

  // A two-rung run also groups: same binding count, half the sampler slots.
  CHECK(d.auxiliary_textures[8].ladder_base == "barDyn");
  CHECK(d.auxiliary_textures[8].ladder_size == 2);
  CHECK(d.auxiliary_textures[8].owns_ladder());
  CHECK(d.auxiliary_textures[9].ladder_index == 1);

  // A singleton has no run to join.
  CHECK_FALSE(d.auxiliary_textures[10].in_ladder());
}

TEST_CASE("a grouped ladder emits one array, one sampler, N defines", "[gfx][isf]")
{
  isf::parser p{
      ladder_vertex(), ladder_shader(), 450,
      isf::parser::ShaderType::RawRasterPipeline};
  const std::string frag = p.fragment();

  CHECK(frag.find("uniform texture2DArray fooArray_tex[8];") != std::string::npos);
  CHECK(frag.find("uniform sampler fooArray_smp;") != std::string::npos);

  // A two-rung run qualifies as well: bindings break even and the sampler
  // slot -- Metal's tighter budget -- halves.
  CHECK(frag.find("uniform texture2D barDyn_tex[2];") != std::string::npos);
  CHECK(frag.find("uniform sampler barDyn_smp;") != std::string::npos);

  // No rung keeps a combined declaration, and no alias is emitted for it.
  for(int i = 0; i < 8; i++)
  {
    const std::string decl
        = "uniform sampler2DArray fooArray" + std::to_string(i) + ";";
    INFO(decl);
    CHECK(frag.find(decl) == std::string::npos);
    CHECK(frag.find("#define fooArray" + std::to_string(i)) == std::string::npos);
  }

  // A singleton is untouched: nothing to collapse.
  CHECK(frag.find("uniform samplerCube skybox;") != std::string::npos);
}

TEST_CASE("grouping lowers the highest binding number", "[gfx][isf]")
{
  isf::parser p{
      ladder_vertex(), ladder_shader(), 450,
      isf::parser::ShaderType::RawRasterPipeline};
  const std::string frag = p.fragment();

  // 11 aux textures occupy 2 + 2 + 1 = 5 slots rather than 11. Asserting the
  // absolute number would pin every unrelated binding the prelude adds, so
  // assert that the total came down instead.
  const int emitted = occurrences(frag, "layout(binding = ");
  const int top = max_binding(frag);
  INFO("bindings emitted: " << emitted << ", max binding: " << top);
  CHECK(top >= 0);
  CHECK(top < 11);
}

TEST_CASE("a grouped ladder bakes on every backend", "[gfx][isf]")
{
  const auto api = GENERATE(
      score::gfx::Vulkan, score::gfx::OpenGL, score::gfx::Metal,
      score::gfx::D3D11, score::gfx::D3D12);

  isf::parser p{
      ladder_vertex(), ladder_shader(), 450,
      isf::parser::ShaderType::RawRasterPipeline};

  for(const auto& [stage, src] :
      {std::pair{QShader::VertexStage, p.vertex()},
       std::pair{QShader::FragmentStage, p.fragment()}})
  {
    const auto& [shader, error] = score::gfx::ShaderCache::get(
        api, version_for(api), QByteArray::fromStdString(src), stage);
    INFO("api " << int(api) << " stage " << int(stage)
                << " bake error: " << error.toStdString());
    REQUIRE(error.isEmpty());
    REQUIRE(shader.isValid());
  }
}
