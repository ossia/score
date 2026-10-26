// Shader inputs named `mouse` / `touch` take a window device's pointer
// addresses by default: explicit addresses, shown on the inlet and changeable
// there, the same mechanism in every shader format (ISFProcess.hpp
// setupISFModelPorts). The address follows what the format defines:
// - vertexshaderart's `mouse` is clip space, -1..1, y up (greggman/
//   vertexshaderart src/js/main.js recordMouseMove, src/index.html "mouse
//   position normalized (-1 to 1)"): Window:/cursor/clip;
// - an ISF `mouse` keeps Window:/cursor/gl (0..1, y up);
// - an image input named `touch`, in either format, reads Window:/touch/history
//   (vertexshaderart's 32x240 history) through a 32x240 RGBA32F NEAREST target.
// Without a window device nothing is bound.
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>

#include <State/Address.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Gfx/Filter/Metadata.hpp>
#include <Gfx/TexturePort.hpp>
#include <Gfx/VSA/Metadata.hpp>
#include <Gfx/Window/WindowSettings.hpp>
#include <Gfx/WindowDevice.hpp>

#include <ossia/network/base/device.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QTemporaryDir>

namespace
{
constexpr const char* kVsa = R"_(/*{
  "ISFVSN": "2", "MODE": "VERTEX_SHADER_ART", "POINT_COUNT": 3,
  "INPUTS": [ { "LABEL": "Touch", "NAME": "touch", "TYPE": "image" },
              { "LABEL": "Mouse", "NAME": "mouse", "TYPE": "point2D", "DEFAULT": [0.0, 0.0], "MIN": [0.0, 0.0], "MAX": [1.0, 1.0] } ]
}*/
void main() {
  vec4 t = texture(touch, vec2(0.));
  gl_Position = vec4(mouse + t.xy, 0., 1.);
  v_color = vec4(1.);
}
)_";

constexpr const char* kIsf = R"_(/*{
  "ISFVSN": "2",
  "INPUTS": [ { "NAME": "touch", "TYPE": "image" },
              { "NAME": "mouse", "TYPE": "point2D" } ]
}*/
void main() { gl_FragColor = IMG_NORM_PIXEL(touch, mouse); }
)_";

QString writeShader(const QTemporaryDir& dir, const char* name, const char* text)
{
  const QString path = dir.filePath(name);
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly))
    return {};
  f.write(text);
  return path;
}

template <typename Model>
Process::ProcessModel* make(
    const score::GUIApplicationContext& ctx, score::Document& doc, const QString& path)
{
  auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
  auto* fac = pl.get(Metadata<ConcreteKey_k, Model>::get());
  REQUIRE(fac);
  return fac->make(
      TimeVal::fromMsecs(1000.), path, Id<Process::ProcessModel>{1}, doc.context(),
      new QObject{&doc.model()});
}

Process::Inlet* inlet(const Process::ProcessModel& p, const char* name)
{
  for(auto* in : p.inlets())
    if(in->name() == name)
      return in;
  return nullptr;
}

QString address(const Process::ProcessModel& p, const char* name)
{
  auto* in = inlet(p, name);
  if(!in)
    for(auto* i : p.inlets())
      UNSCOPED_INFO("inlet: " << i->name().toStdString());
  REQUIRE(in);
  return in->address().address.toString();
}

void addWindow(score::Document& doc, const QString& name)
{
  Device::DeviceSettings s;
  s.name = name;
  s.protocol = Gfx::WindowProtocolFactory::static_concreteKey();
  s.deviceSpecificSettings = QVariant::fromValue(Gfx::WindowSettings{});
  auto& plug = doc.context().plugin<Explorer::DeviceDocumentPlugin>();
  plug.list().addDevice(new Gfx::WindowDevice{s, doc.context()});
}

void checkTouchTarget(const Process::ProcessModel& p)
{
  auto* t = qobject_cast<Gfx::TextureInlet*>(inlet(p, "touch"));
  REQUIRE(t);
  REQUIRE(t->renderSize());
  CHECK(*t->renderSize() == QSize{32, 240});
  REQUIRE(t->textureFormat());
  CHECK(*t->textureFormat() == ossia::texture_format::RGBA32F);
  REQUIRE(t->textureFilter());
  CHECK(*t->textureFilter() == ossia::texture_filter::NEAREST);
  REQUIRE(t->textureAddressMode());
  CHECK(*t->textureAddressMode() == ossia::texture_address_mode::CLAMP_TO_EDGE);
}
}

TEST_CASE(
    "mouse and touch inputs take the window's pointer addresses",
    "[gfx][vsa][isf][touch][mouse]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString vsaPath = writeShader(dir, "vsa-probe.vs", kVsa);
    const QString isfPath = writeShader(dir, "isf-probe.fs", kIsf);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    addWindow(*doc, "Win");

    auto* vsa = make<Gfx::VSA::Model>(ctx, *doc, vsaPath);
    REQUIRE(vsa);
    CHECK(address(*vsa, "mouse") == "Win:/cursor/clip");
    CHECK(address(*vsa, "touch") == "Win:/touch/history");
    checkTouchTarget(*vsa);

    auto* isf = make<Gfx::Filter::Model>(ctx, *doc, isfPath);
    REQUIRE(isf);
    CHECK(address(*isf, "mouse") == "Win:/cursor/gl");
    CHECK(address(*isf, "touch") == "Win:/touch/history");
    checkTouchTarget(*isf);
  });
}

TEST_CASE("without a window device nothing is bound", "[gfx][vsa][isf][touch][mouse]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString vsaPath = writeShader(dir, "vsa-probe.vs", kVsa);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* vsa = make<Gfx::VSA::Model>(ctx, *doc, vsaPath);
    REQUIRE(vsa);
    CHECK(address(*vsa, "mouse").isEmpty());
    CHECK(address(*vsa, "touch").isEmpty());
    auto* t = qobject_cast<Gfx::TextureInlet*>(inlet(*vsa, "touch"));
    REQUIRE(t);
    CHECK_FALSE(t->renderSize());
  });
}
