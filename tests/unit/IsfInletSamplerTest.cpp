// The sampler of a texture input, field by field: filter, address mode and
// mipmaps each come from the inlet when the user set them there, otherwise
// from the shader's JSON when it declares them (WRAP, FILTER, MIPMAP_MODE),
// otherwise from the engine's defaults. COMPARE is the shader's alone
// (shader board U34). The inspector shows what an unset field resolves to and
// offers it back as "Auto"; documents saved before keep the shader's sampler.
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>

#include <Gfx/Filter/Metadata.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/TexturePort.hpp>

#include <isf.hpp>

#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/widgets/Layout.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/dataflow/port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QTemporaryDir>
#include <QWidget>

#include <memory>

namespace
{
//! The inspector field of the row labelled `row`.
QWidget* rowField(QWidget& parent, Inspector::Layout& lay, const QString& row)
{
  for(int i = 0; i < lay.rowCount(); i++)
  {
    auto* label = lay.itemAt(i, QFormLayout::LabelRole);
    auto* field = lay.itemAt(i, QFormLayout::FieldRole);
    if(!label || !field)
      continue;
    auto* l = qobject_cast<QLabel*>(label->widget());
    if(l && l->text() == row)
      return field->widget();
  }
  return nullptr;
}

struct Inspector_
{
  QWidget parent;
  Inspector::Layout lay{&parent};
  QObject context;
  Inspector_(
      const score::GUIApplicationContext& ctx, const score::DocumentContext& doc,
      const Process::Inlet& inlet)
  {
    auto* fac = ctx.interfaces<Process::PortFactoryList>().get(inlet.concreteKey());
    REQUIRE(fac);
    fac->setupInletInspector(inlet, doc, &parent, lay, &context);
  }
  QComboBox* combo(const QString& row)
  {
    auto* w = qobject_cast<QComboBox*>(rowField(parent, lay, row));
    REQUIRE(w);
    return w;
  }
};

QString writeShader(const QTemporaryDir& dir, const char* name, const char* text)
{
  const QString path = dir.filePath(name);
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly))
    return {};
  f.write(text);
  return path;
}

constexpr const char* kTwoInputs = R"_(/*{ "ISFVSN": "2", "INPUTS": [
  { "NAME": "declared", "TYPE": "image", "WRAP": "repeat", "FILTER": "nearest" },
  { "NAME": "plain", "TYPE": "image" },
  { "NAME": "shadow", "TYPE": "image", "COMPARE": "less" } ] }*/
void main() { gl_FragColor = IMG_NORM_PIXEL(declared, isf_FragNormCoord) + IMG_NORM_PIXEL(plain, isf_FragNormCoord); }
)_";

Process::ProcessModel* makeFilter(
    const score::GUIApplicationContext& ctx, score::Document& doc, const QString& path)
{
  auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
  auto* fac = pl.get(Metadata<ConcreteKey_k, Gfx::Filter::Model>::get());
  REQUIRE(fac);
  return fac->make(
      TimeVal::fromMsecs(1000.), path, Id<Process::ProcessModel>{1}, doc.context(),
      new QObject{&doc.model()});
}

const Gfx::TextureInlet* inlet(const Process::ProcessModel& p, const char* name)
{
  for(auto* in : p.inlets())
    if(auto* t = qobject_cast<Gfx::TextureInlet*>(in); t && t->name() == name)
      return t;
  return nullptr;
}
}

TEST_CASE("a sampler resolves field by field", "[gfx][isf][sampler][U34]")
{
  using namespace score::gfx;
  isf::sampler_config cfg;
  cfg.filter = "nearest";
  cfg.wrap = "repeat";

  // Nothing set on the inlet: the shader's fields, and its defaults (no
  // mipmaps) for what it leaves out.
  RenderTargetSpecs unset;
  resolveSamplerFields(unset, cfg);
  CHECK(unset.mag_filter == QRhiSampler::Nearest);
  CHECK(unset.min_filter == QRhiSampler::Nearest);
  CHECK(unset.address_u == QRhiSampler::Repeat);
  CHECK(unset.address_w == QRhiSampler::Repeat);
  CHECK(unset.mipmap_mode == QRhiSampler::None);

  // The filter set on the inlet wins; the address mode is still the shader's.
  RenderTargetSpecs filterSet;
  filterSet.filter_set = true;
  filterSet.mag_filter = filterSet.min_filter = QRhiSampler::Linear;
  resolveSamplerFields(filterSet, cfg);
  CHECK(filterSet.mag_filter == QRhiSampler::Linear);
  CHECK(filterSet.address_u == QRhiSampler::Repeat);

  // A shader that declares nothing leaves the inlet's values.
  RenderTargetSpecs plain;
  plain.address_u = plain.address_v = plain.address_w = QRhiSampler::Mirror;
  resolveSamplerFields(plain, isf::sampler_config{});
  CHECK(plain.address_u == QRhiSampler::Mirror);
  CHECK(plain.mag_filter == QRhiSampler::Linear);
}

TEST_CASE("an inlet sends its unset sampler fields as unset", "[gfx][isf][sampler][U34]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    Gfx::TextureInlet in{"t", Id<Process::Port>{0}, nullptr};
    QObject exec_context;
    ossia::texture_inlet exec;
    in.setupExecution(exec, &exec_context);
    auto specs = score::gfx::toRenderTargetSpecs(exec.data);
    CHECK_FALSE(specs.filter_set);
    CHECK_FALSE(specs.address_set);
    CHECK_FALSE(specs.mipmap_set);
    // Unset fields carry the engine's defaults.
    CHECK(specs.mag_filter == QRhiSampler::Linear);
    CHECK(specs.address_u == QRhiSampler::ClampToEdge);
    CHECK(specs.mipmap_mode == QRhiSampler::None);

    in.setTextureFilter(ossia::texture_filter::NEAREST);
    in.setTextureAddressMode(ossia::texture_address_mode::MIRROR);
    specs = score::gfx::toRenderTargetSpecs(exec.data);
    CHECK(specs.filter_set);
    CHECK(specs.mag_filter == QRhiSampler::Nearest);
    CHECK(specs.address_set);
    CHECK(specs.address_v == QRhiSampler::Mirror);
    CHECK_FALSE(specs.mipmap_set);

    in.setTextureFilter(std::nullopt);
    specs = score::gfx::toRenderTargetSpecs(exec.data);
    CHECK_FALSE(specs.filter_set);
  });
}

TEST_CASE("the inspector shows what an unset sampler field resolves to", "[gfx][isf][sampler][U34]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = writeShader(dir, "inputs.fs", kTwoInputs);
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = makeFilter(ctx, *doc, path);
    REQUIRE(proc);
    auto* declared = inlet(*proc, "declared");
    auto* plain = inlet(*proc, "plain");
    auto* shadow = inlet(*proc, "shadow");
    REQUIRE(declared);
    REQUIRE(plain);
    REQUIRE(shadow);
    CHECK(declared->shaderSampler() != nullptr);
    CHECK_FALSE(declared->textureFilter());

    {
      Inspector_ i{ctx, doc->context(), *declared};
      auto* filter = i.combo("Filter");
      CHECK(filter->isEnabled());
      CHECK(filter->currentIndex() == 0);
      CHECK(filter->currentText().contains("Shader"));
      CHECK(filter->currentText().contains("Nearest"));
      auto* address = i.combo("Address mode");
      CHECK(address->currentText().contains("Repeat"));
      // The mipmaps the shader leaves to the defaults.
      auto* mips = i.combo("Mipmaps");
      CHECK(mips->currentText().contains("Default"));
    }
    {
      Inspector_ i{ctx, doc->context(), *plain};
      CHECK(i.combo("Filter")->currentText().contains("Default"));
      CHECK(i.combo("Filter")->currentText().contains("Linear"));
    }
    {
      // A comparison sampler is the shader's.
      Inspector_ i{ctx, doc->context(), *shadow};
      CHECK_FALSE(i.combo("Filter")->isEnabled());
      CHECK_FALSE(i.combo("Address mode")->isEnabled());
      CHECK_FALSE(i.combo("Mipmaps")->isEnabled());
    }
  });
}

TEST_CASE("a sampler override is saved and loaded", "[gfx][isf][sampler][U34]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = writeShader(dir, "inputs.fs", kTwoInputs);
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = makeFilter(ctx, *doc, path);
    REQUIRE(proc);
    auto* declared = const_cast<Gfx::TextureInlet*>(inlet(*proc, "declared"));
    REQUIRE(declared);
    declared->setTextureFilter(ossia::texture_filter::LINEAR);

    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    const auto check = [&](const Process::ProcessModel& loaded) {
      auto* d = inlet(loaded, "declared");
      auto* p = inlet(loaded, "plain");
      REQUIRE(d);
      REQUIRE(p);
      CHECK(d->textureFilter() == ossia::texture_filter::LINEAR);
      CHECK_FALSE(d->textureAddressMode());
      CHECK_FALSE(p->textureFilter());
      // Loaded inlets still know their shader's sampler: the inspector shows
      // it (it was lost on reload, the inlet's own flag not being saved).
      CHECK(d->shaderSampler() != nullptr);
      Inspector_ i{ctx, doc->context(), *d};
      CHECK(i.combo("Address mode")->currentText().contains("Shader"));
      CHECK(i.combo("Filter")->currentIndex() != 0);
    };

    {
      JSONReader r;
      r.readFrom(*proc);
      const rapidjson::Document json = readJson(r.toByteArray());
      JSONObject::Deserializer des{json};
      std::unique_ptr<Process::ProcessModel> loaded{
          deserialize_interface(pl, des, doc->context(), &doc->model())};
      REQUIRE(loaded);
      check(*loaded);
    }
    {
      const QByteArray bytes = score::marshall<DataStream>(*proc);
      std::unique_ptr<Process::ProcessModel> loaded{deserialize_interface(
          pl, DataStream::Deserializer{bytes}, doc->context(), &doc->model())};
      REQUIRE(loaded);
      check(*loaded);
    }
  });
}

TEST_CASE("a document saved before keeps the shader's sampler", "[gfx][isf][sampler][U34]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = writeShader(dir, "inputs.fs", kTwoInputs);
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = makeFilter(ctx, *doc, path);
    REQUIRE(proc);

    // A texture inlet as documents saved it until now: every field written,
    // with the defaults for the ones never touched, and no record of which
    // the user set. A value other than the default was the user's choice.
    JSONReader r;
    r.readFrom(*proc);
    rapidjson::Document json = readJson(r.toByteArray());
    auto& inlets = json["Inlets"];
    REQUIRE(inlets.IsArray());
    int edited = 0;
    for(auto& in : inlets.GetArray())
    {
      if(!in.HasMember("Custom") || !in.HasMember("Filter"))
        continue;
      for(const char* key : {"FilterSet", "AddressModeSet", "MipmapModeSet"})
        in.RemoveMember(key);
      in["Filter"] = (int)ossia::texture_filter::LINEAR;
      in["AddressMode"] = (int)ossia::texture_address_mode::CLAMP_TO_EDGE;
      in["MipmapMode"] = (int)ossia::texture_filter::NONE;
      if(in["Custom"] == "plain")
        in["AddressMode"] = (int)ossia::texture_address_mode::MIRROR;
      edited++;
    }
    REQUIRE(edited == 3);

    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    JSONObject::Deserializer des{json};
    std::unique_ptr<Process::ProcessModel> loaded{
        deserialize_interface(pl, des, doc->context(), &doc->model())};
    REQUIRE(loaded);
    auto* d = inlet(*loaded, "declared");
    auto* p = inlet(*loaded, "plain");
    REQUIRE(d);
    REQUIRE(p);
    // Defaults: unset, so the shader's NEAREST / REPEAT apply.
    CHECK_FALSE(d->textureFilter());
    CHECK_FALSE(d->textureAddressMode());
    CHECK_FALSE(d->textureMipmapMode());
    // The user's mirror is kept.
    CHECK(p->textureAddressMode() == ossia::texture_address_mode::MIRROR);
    CHECK_FALSE(p->textureFilter());
  });
}
