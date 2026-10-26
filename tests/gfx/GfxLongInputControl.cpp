// A `long` input without VALUES is a number, shown as a spin box over the MIN
// and MAX it gives. A bare one, or one with a single bound, became an
// arbitrary 0 / 1 / 2 combo box, its DEFAULT clamped to 2 (shader board U11);
// a geometry filter had no spin box at all. Same rule in the ISF filter, the
// compute shader and the geometry filter processes.
#include "GfxProcessDoc.hpp"

#include <Process/Dataflow/WidgetInlets.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <Gfx/Filter/Process.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <memory>

using namespace score::test;
using namespace score::test::gfxproc;

namespace
{
constexpr auto UUID_FILTER = "74ca45ff-92c9-44a0-8f1a-754dea05ee1b";
constexpr auto UUID_GEOMFILTER = "27d3cc85-a4b0-4924-8fde-71c337b40f59";
constexpr auto UUID_CSF = "a5bbffe0-93d2-4e70-995c-cf46c2c43520";

constexpr auto LONGS = R"_(
    { "NAME": "bare", "TYPE": "long", "DEFAULT": 5 },
    { "NAME": "atleast", "TYPE": "long", "MIN": 2, "DEFAULT": 7 },
    { "NAME": "choice", "TYPE": "long", "VALUES": [ 10, 20, 30 ], "LABELS": [ "a", "b", "c" ], "DEFAULT": 1 })_";

Process::ControlInlet* control(const Process::ProcessModel& p, const char* name)
{
  for(auto* inl : p.inlets())
    if(inl->name() == QLatin1String(name))
      return dynamic_cast<Process::ControlInlet*>(inl);
  return nullptr;
}
}

TEST_CASE("a long input without VALUES is a spin box", "[gfx][process][U11]")
{
  struct Kind
  {
    const char* uuid;
    const char* file;
    std::string header, body;
  };
  const auto k = GENERATE(
      Kind{UUID_FILTER, "f.fs", R"_("ISFVSN": "2.0", "INPUTS": [)_",
           "void main() { gl_FragColor = vec4(float(bare + atleast + choice)); }"},
      Kind{UUID_CSF, "c.cs",
           R"_("ISFVSN": "2.0", "MODE": "COMPUTE_SHADER",
    "PASSES": [ { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } } ],
    "RESOURCES": [ { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "4", "HEIGHT": "4" }, )_",
           "void main() { }"},
      Kind{UUID_GEOMFILTER, "g.glsl", R"_("ISFVSN": "2", "MODE": "GEOMETRY_FILTER", "INPUTS": [)_",
           "void process_vertex(inout vec3 position, inout vec3 normal, inout vec2 uv, "
           "inout vec3 tangent, inout vec4 color) { }"});
  CAPTURE(k.file);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.path() + "/" + k.file;
  {
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(QByteArray::fromStdString(
        "/*{ " + k.header + LONGS + " ] }*/\n" + k.body + "\n"));
  }

  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);
    auto* proc = add_process(ctx, *doc, k.uuid, path);
    REQUIRE(proc != nullptr);

    auto* bare = control(*proc, "bare");
    auto* atleast = control(*proc, "atleast");
    auto* choice = control(*proc, "choice");
    REQUIRE(bare);
    REQUIRE(atleast);
    REQUIRE(choice);
    CHECK(dynamic_cast<Process::IntSpinBox*>(bare));
    CHECK(dynamic_cast<Process::IntSpinBox*>(atleast));
    CHECK(dynamic_cast<Process::ComboBox*>(choice));
    CHECK(ossia::convert<int>(bare->value()) == 5);
    CHECK(ossia::convert<int>(atleast->value()) == 7);
    CHECK(ossia::convert<int>(choice->value()) == 20);
  });
}

// A document saved before the spin box keeps the 0 / 1 / 2 combo box it was
// saved with, and its value: loading restores the saved ports, it does not
// rebuild them from the shader. Re-reading the shader then makes it a spin
// box, carrying the value over.
TEST_CASE("an old document keeps its long combo box until the shader is re-read", "[gfx][process][U11]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.path() + "/f.fs";
  {
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(R"_(/*{ "ISFVSN": "2.0", "INPUTS": [ { "NAME": "bare", "TYPE": "long", "DEFAULT": 5 } ] }*/
void main() { gl_FragColor = vec4(float(bare)); }
)_");
  }

  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);
    auto* proc = add_process(ctx, *doc, UUID_FILTER, path);
    REQUIRE(proc != nullptr);
    auto* bare = control(*proc, "bare");
    REQUIRE(bare);

    // What the old code made of this input, and saved: a combo box over
    // 0 / 1 / 2 whose DEFAULT was clamped to 2.
    Process::ComboBox old{
        {{"0", 0}, {"1", 1}, {"2", 2}}, 2, "bare", bare->id(), nullptr};
    JSONReader portReader;
    portReader.readFrom(old);
    const rapidjson::Document portJson = readJson(portReader.toByteArray());

    JSONReader reader;
    reader.readFrom(*proc);
    rapidjson::Document json = readJson(reader.toByteArray());
    auto& saved = json["Inlets"];
    REQUIRE(saved.IsArray());
    bool replaced = false;
    for(auto& inl : saved.GetArray())
      if(inl.HasMember("Custom") && inl["Custom"] == "bare")
      {
        inl.CopyFrom(portJson, json.GetAllocator());
        replaced = true;
      }
    REQUIRE(replaced);

    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    JSONObject::Deserializer des{json};
    std::unique_ptr<Process::ProcessModel> loaded{
        deserialize_interface(pl, des, doc->context(), &doc->model())};
    REQUIRE(loaded);
    auto* loadedBare = control(*loaded, "bare");
    REQUIRE(loadedBare);
    CHECK(dynamic_cast<Process::ComboBox*>(loadedBare));
    CHECK(ossia::convert<int>(loadedBare->value()) == 2);

    auto* filter = dynamic_cast<Gfx::Filter::Model*>(loaded.get());
    REQUIRE(filter);
    (void)filter->setProgram(filter->program());
    auto* reread = control(*loaded, "bare");
    REQUIRE(reread);
    CHECK(dynamic_cast<Process::IntSpinBox*>(reread));
    CHECK(ossia::convert<int>(reread->value()) == 2);
  });
}
