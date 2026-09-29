// ISF v1 PERSISTENT_BUFFERS is upgraded by the libisf parser to the v2 model:
// every pass whose TARGET is a listed buffer becomes PERSISTENT, and the v1
// buffer's WIDTH / HEIGHT / FLOAT go to that pass where it does not set its
// own. Downstream code only ever sees isf::pass::persistent.
//
// Pure CPU: parse the header / the whole shader, inspect the descriptor.
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{
isf::descriptor header(const std::string& json)
{
  return isf::parser::parse_isf_header("/*" + json + "*/\nvoid main() {}\n").second;
}
}

// gs_34137.1 and 9 other library presets: a name list, the pass only TARGETs it.
TEST_CASE("PERSISTENT_BUFFERS list makes the targeting pass persistent", "[isf][persistent]")
{
  const auto d = header(R"_({
  "CATEGORIES" : [ "Automatically Converted" ],
  "INPUTS" : [ ],
  "PERSISTENT_BUFFERS" : [ "backbuffer" ],
  "PASSES" : [ { "TARGET" : "backbuffer" } ]
})_");
  REQUIRE(d.passes.size() == 1);
  CHECK(d.passes[0].target == "backbuffer");
  CHECK(d.passes[0].persistent);
  CHECK_FALSE(d.passes[0].float_storage);
  CHECK(d.passes[0].width_expression.empty());
  CHECK(d.passes[0].height_expression.empty());
}

// gs_4478.0, gs_51233.10 and 5 others: the pass also says PERSISTENT itself.
TEST_CASE("PERSISTENT_BUFFERS agrees with a pass already PERSISTENT", "[isf][persistent]")
{
  const auto d = header(R"_({
  "PERSISTENT_BUFFERS" : [ "bb" ],
  "PASSES" : [ { "TARGET" : "bb", "PERSISTENT" : true } ]
})_");
  REQUIRE(d.passes.size() == 1);
  CHECK(d.passes[0].persistent);
}

// Mitti/FakeMotionBlur: the pass carries FLOAT, which is kept.
TEST_CASE("PERSISTENT_BUFFERS keeps the pass FLOAT", "[isf][persistent]")
{
  const auto d = header(R"_({
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" },
              { "NAME": "blurAmount", "TYPE": "float" } ],
  "PERSISTENT_BUFFERS": [ "bufferVariableNameA" ],
  "PASSES": [ { "TARGET": "bufferVariableNameA", "FLOAT": true } ]
})_");
  REQUIRE(d.passes.size() == 1);
  CHECK(d.passes[0].persistent);
  CHECK(d.passes[0].float_storage);
}

TEST_CASE(
    "PERSISTENT_BUFFERS only touches the passes that target a listed buffer",
    "[isf][persistent]")
{
  const auto d = header(R"_({
  "PERSISTENT_BUFFERS" : [ "history" ],
  "PASSES" : [ { "TARGET" : "blurred" }, { "TARGET" : "history" }, { } ]
})_");
  REQUIRE(d.passes.size() == 3);
  CHECK_FALSE(d.passes[0].persistent);
  CHECK(d.passes[1].persistent);
  CHECK_FALSE(d.passes[2].persistent);
}

TEST_CASE("PERSISTENT_BUFFERS object form carries size and format", "[isf][persistent]")
{
  const auto d = header(R"_({
  "PERSISTENT_BUFFERS" : {
    "a" : { "WIDTH" : "$WIDTH/2", "HEIGHT" : 16, "FLOAT" : true },
    "b" : { "WIDTH" : 8, "HEIGHT" : "$HEIGHT" },
    "c" : { }
  },
  "PASSES" : [
    { "TARGET" : "a" },
    { "TARGET" : "b", "WIDTH" : "$WIDTH", "FLOAT" : true },
    { "TARGET" : "c" }
  ]
})_");
  REQUIRE(d.passes.size() == 3);

  CHECK(d.passes[0].persistent);
  CHECK(d.passes[0].width_expression == "$WIDTH/2");
  CHECK(d.passes[0].height_expression == "16");
  CHECK(d.passes[0].float_storage);

  // The pass's own WIDTH and FLOAT win over the buffer's.
  CHECK(d.passes[1].persistent);
  CHECK(d.passes[1].width_expression == "$WIDTH");
  CHECK(d.passes[1].height_expression == "$HEIGHT");
  CHECK(d.passes[1].float_storage);

  CHECK(d.passes[2].persistent);
  CHECK(d.passes[2].width_expression.empty());
  CHECK(d.passes[2].height_expression.empty());
  CHECK_FALSE(d.passes[2].float_storage);
}

TEST_CASE(
    "a PERSISTENT_BUFFERS entry no pass targets is ignored", "[isf][persistent]")
{
  const auto d = header(R"_({
  "PERSISTENT_BUFFERS" : [ "unused", 3, "" ],
  "PASSES" : [ { "TARGET" : "used" } ]
})_");
  REQUIRE(d.passes.size() == 1);
  CHECK_FALSE(d.passes[0].persistent);
  CHECK(d.pass_targets == std::vector<std::string>{"used"});
}

// The full ISF parse sees the upgraded pass, and writing the shader back out
// produces the v2 form.
TEST_CASE("a v1 PERSISTENT_BUFFERS shader parses and writes back as v2", "[isf][persistent]")
{
  const std::string src = R"_(/*
{
  "INPUTS" : [ ],
  "PERSISTENT_BUFFERS" : [ "backbuffer" ],
  "PASSES" : [ { "TARGET" : "backbuffer" } ]
}
*/
void main()
{
  gl_FragColor = IMG_NORM_PIXEL(backbuffer, isf_FragNormCoord) * 0.9;
}
)_";
  const isf::parser p{{}, src, 450, isf::parser::ShaderType::ISF};
  const auto d = p.data();
  REQUIRE(d.passes.size() == 1);
  CHECK(d.passes[0].persistent);
  CHECK(p.fragment().find("sampler2D backbuffer") != std::string::npos);

  const auto v2 = p.write_isf();
  CHECK(v2.find("PERSISTENT_BUFFERS") == std::string::npos);
  CHECK(v2.find("\"PERSISTENT\": true") != std::string::npos);
}
