// A point4D input is a control holding a vec4: its DEFAULT is the control's
// value and init, its MIN / MAX the domain. The four numbers are kept as they
// are (no colour conversion, no 0..1 normalisation): a matrix column may be
// negative or larger than one. They survive saving the document in both
// formats and loading it back, in the ISF filter, the compute shader and the
// geometry filter processes, and an ISF filter re-reading its shader keeps the
// value.
#include "GfxProcessDoc.hpp"

#include <Process/Dataflow/Port.hpp>

#include <Gfx/Filter/Process.hpp>

#include <score_test/Document.hpp>

#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QFile>
#include <QTemporaryDir>

using namespace score::test;
using namespace score::test::gfxproc;

namespace
{
constexpr auto UUID_FILTER = "74ca45ff-92c9-44a0-8f1a-754dea05ee1b";
constexpr auto UUID_GEOMFILTER = "27d3cc85-a4b0-4924-8fde-71c337b40f59";
constexpr auto UUID_CSF = "a5bbffe0-93d2-4e70-995c-cf46c2c43520";

constexpr auto POINT4D = R"_(
    { "NAME": "level", "TYPE": "float", "DEFAULT": 0.5 },
    { "NAME": "column", "TYPE": "point4D", "DEFAULT": [1.0, 0.0, -0.5, 0.0],
      "MIN": [-10.0, -10.0, -10.0, -10.0], "MAX": [10.0, 10.0, 10.0, 20.0] })_";

const ossia::vec4f k_default{1.f, 0.f, -0.5f, 0.f};
const ossia::vec4f k_min{-10.f, -10.f, -10.f, -10.f};
const ossia::vec4f k_max{10.f, 10.f, 10.f, 20.f};
// Negative and above one: what a colour or a 0..1 slider could not hold.
const ossia::vec4f k_set{-3.5f, 0.25f, 7.f, 12.f};

Process::ControlInlet* control(const Process::ProcessModel& p, const char* name)
{
  for(auto* inl : p.inlets())
    if(inl->name() == QLatin1String(name))
      return dynamic_cast<Process::ControlInlet*>(inl);
  return nullptr;
}

Process::ProcessModel* process_of(score::Document& doc, const char* uuid)
{
  const auto key = UuidKey<Process::ProcessModel>::fromString(QString::fromLatin1(uuid));
  for(auto& p : base_interval(doc).processes)
    if(p.concreteKey() == key)
      return &p;
  return nullptr;
}

bool is_vec4(const ossia::value& v, const ossia::vec4f& expected)
{
  return v.get_type() == ossia::val_type::VEC4F
         && ossia::convert<ossia::vec4f>(v) == expected;
}

struct Kind
{
  const char* uuid;
  const char* file;
  std::string header, body;
};

Kind kind()
{
  return GENERATE(
      Kind{UUID_FILTER, "f.fs", R"_("ISFVSN": "2.0", "INPUTS": [)_",
           "void main() { gl_FragColor = column * level; }"},
      Kind{UUID_CSF, "c.cs",
           R"_("ISFVSN": "2.0", "MODE": "COMPUTE_SHADER",
    "PASSES": [ { "LOCAL_SIZE": [1, 1, 1], "EXECUTION_MODEL": { "TYPE": "MANUAL", "WORKGROUPS": [1, 1, 1] } } ],
    "RESOURCES": [ { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "4", "HEIGHT": "4" }, )_",
           "void main() { imageStore(outputImage, ivec2(0), column * level); }"},
      Kind{UUID_GEOMFILTER, "g.glsl", R"_("ISFVSN": "2", "MODE": "GEOMETRY_FILTER", "INPUTS": [)_",
           "void process_vertex(inout vec3 position, inout vec3 normal, inout vec2 uv, "
           "inout vec3 tangent, inout vec4 color) { color = column * level; }"});
}

QString write_shader(const QTemporaryDir& dir, const Kind& k)
{
  const QString path = dir.path() + "/" + k.file;
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly))
    return {};
  f.write(QByteArray::fromStdString(
      "/*{ " + k.header + POINT4D + " ] }*/\n" + k.body + "\n"));
  return path;
}
}

TEST_CASE("a point4D input is a vec4 control at its DEFAULT within MIN and MAX", "[gfx][process][point4D]")
{
  const Kind k = kind();
  CAPTURE(k.file);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = write_shader(dir, k);
  REQUIRE_FALSE(path.isEmpty());

  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);
    auto* proc = add_process(ctx, *doc, k.uuid, path);
    REQUIRE(proc != nullptr);

    auto* column = control(*proc, "column");
    REQUIRE(column);
    CHECK(is_vec4(column->value(), k_default));
    CHECK(is_vec4(column->init(), k_default));
    const auto& dom = column->domain().get();
    CHECK(dom.convert_min<ossia::vec4f>() == k_min);
    CHECK(dom.convert_max<ossia::vec4f>() == k_max);

    // The control after it is still there: the vec4 did not shift the ports.
    CHECK(control(*proc, "level"));
  });
}

TEST_CASE("a point4D control keeps its four numbers through save and reload", "[gfx][process][point4D]")
{
  const Kind k = kind();
  CAPTURE(k.file);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = write_shader(dir, k);
  REQUIRE_FALSE(path.isEmpty());

  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);
    auto* proc = add_process(ctx, *doc, k.uuid, path);
    REQUIRE(proc != nullptr);
    auto* column = control(*proc, "column");
    REQUIRE(column);
    column->setValue(k_set);

    for(const bool json : {true, false})
    {
      CAPTURE(json);
      score::Document* reloaded
          = json ? reload_via_json(ctx, *doc) : reload_via_bytes(ctx, *doc);
      REQUIRE(reloaded != nullptr);
      auto* rproc = process_of(*reloaded, k.uuid);
      REQUIRE(rproc != nullptr);
      auto* rcolumn = control(*rproc, "column");
      REQUIRE(rcolumn);
      CHECK(is_vec4(rcolumn->value(), k_set));
      CHECK(is_vec4(rcolumn->init(), k_default));
      CHECK(rcolumn->domain().get().convert_max<ossia::vec4f>() == k_max);
    }
  });
}

TEST_CASE("an ISF filter re-reading its shader keeps a point4D value", "[gfx][process][point4D]")
{
  const Kind k{UUID_FILTER, "f.fs", R"_("ISFVSN": "2.0", "INPUTS": [)_",
               "void main() { gl_FragColor = column * level; }"};
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = write_shader(dir, k);
  REQUIRE_FALSE(path.isEmpty());

  run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);
    auto* filter = dynamic_cast<Gfx::Filter::Model*>(add_process(ctx, *doc, UUID_FILTER, path));
    REQUIRE(filter != nullptr);
    auto* column = control(*filter, "column");
    REQUIRE(column);
    column->setValue(k_set);

    (void)filter->setProgram(filter->program());
    auto* reread = control(*filter, "column");
    REQUIRE(reread);
    CHECK(is_vec4(reread->value(), k_set));
  });
}
