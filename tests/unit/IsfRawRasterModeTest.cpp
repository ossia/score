// libisf's side of the raw raster Mode control and of CSF geometry TOPOLOGY:
// the Mode control a raw raster gets offers Geometry as its fourth value, and
// a CSF geometry resource's TOPOLOGY is parsed, written back and validated.
// What the engine draws with them is tests/gfx/GfxRawRasterMode.cpp.
#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QString>

#include <algorithm>
#include <string>

namespace
{
std::string readCorpus(const char* file)
{
  QFile f{QStringLiteral(GFX_TEST_CORPUS_DIR "/") + file};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  return f.readAll().toStdString();
}

std::string csfWithTopology(const std::string& topology)
{
  std::string topologyLine;
  if(!topology.empty())
    topologyLine = R"(      "TOPOLOGY": ")" + topology + "\",\n";
  return R"(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    {
      "NAME": "geo",
      "TYPE": "geometry",
      "VERTEX_COUNT": "3",
)" + topologyLine
         + R"(      "ATTRIBUTES": [
        { "NAME": "position", "SEMANTIC": "position", "TYPE": "vec4", "ACCESS": "write_only" }
      ]
    }
  ],
  "PASSES": [ { "LOCAL_SIZE": [3, 1, 1], "EXECUTION_MODEL": { "TYPE": "PER_VERTEX" } } ]
}*/
void main() { }
)";
}

const isf::geometry_input* firstGeometry(const isf::descriptor& d)
{
  for(const auto& in : d.inputs)
    if(auto* g = ossia::get_if<isf::geometry_input>(&in.data))
      return g;
  return nullptr;
}
}

TEST_CASE("the raw raster Mode control offers Geometry as value 3", "[isf][raster]")
{
  const auto vs = readCorpus("raw-raster-basic.vs");
  const auto fs = readCorpus("raw-raster-basic.fs");
  REQUIRE(!vs.empty());
  REQUIRE(!fs.empty());
  isf::parser p{vs, fs, 450, isf::parser::ShaderType::RawRasterPipeline};
  const auto& ins = p.data().inputs;
  const auto it = std::find_if(
      ins.begin(), ins.end(), [](const isf::input& i) { return i.name == "Mode"; });
  REQUIRE(it != ins.end());
  const auto* l = ossia::get_if<isf::long_input>(&it->data);
  REQUIRE(l);
  REQUIRE(l->labels.size() == 4);
  CHECK(l->labels[0] == "Triangles");
  CHECK(l->labels[1] == "Points");
  CHECK(l->labels[2] == "Lines");
  CHECK(l->labels[3] == "Geometry");
  CHECK(ossia::get<int64_t>(l->values[3]) == 3);
  CHECK(l->def == 0);
}

TEST_CASE("a CSF geometry TOPOLOGY is parsed, written back and validated", "[isf][csf]")
{
  {
    isf::parser p{csfWithTopology("Triangle_Strip"), isf::parser::ShaderType::CSF};
    const auto desc = p.data();
    const auto* g = firstGeometry(desc);
    REQUIRE(g);
    CHECK(g->topology == "triangle_strip");
    CHECK(p.write_isf().find("\"TOPOLOGY\": \"triangle_strip\"") != std::string::npos);
  }
  {
    isf::parser p{csfWithTopology({}), isf::parser::ShaderType::CSF};
    const auto desc = p.data();
    const auto* g = firstGeometry(desc);
    REQUIRE(g);
    CHECK(g->topology.empty());
    CHECK(p.write_isf().find("TOPOLOGY") == std::string::npos);
  }
  CHECK_THROWS_AS(
      (isf::parser{csfWithTopology("quads"), isf::parser::ShaderType::CSF}),
      isf::invalid_file);
}
