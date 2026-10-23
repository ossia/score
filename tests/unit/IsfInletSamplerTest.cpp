// An image input whose JSON declares its sampler (WRAP, FILTER, MIPMAP_MODE,
// COMPARE) gets that sampler, whatever its inlet says (shader board U34): the
// inlet is marked so the inspector shows its filter, address and mipmap
// settings read-only. An input without sampler fields keeps them editable.
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>

#include <Gfx/Filter/Metadata.hpp>
#include <Gfx/TexturePort.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <score/widgets/Layout.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QTemporaryDir>
#include <QWidget>

namespace
{
//! Whether the inspector row labelled `row` is editable for this inlet.
bool rowEnabled(
    const score::GUIApplicationContext& ctx, const score::DocumentContext& doc,
    const Process::Inlet& inlet, const QString& row)
{
  auto* fac = ctx.interfaces<Process::PortFactoryList>().get(inlet.concreteKey());
  REQUIRE(fac);
  QWidget parent;
  Inspector::Layout lay{&parent};
  QObject context;
  fac->setupInletInspector(inlet, doc, &parent, lay, &context);
  for(int i = 0; i < lay.rowCount(); i++)
  {
    auto* label = lay.itemAt(i, QFormLayout::LabelRole);
    auto* field = lay.itemAt(i, QFormLayout::FieldRole);
    if(!label || !field)
      continue;
    auto* l = qobject_cast<QLabel*>(label->widget());
    if(l && l->text() == row)
    {
      REQUIRE(field->widget());
      return field->widget()->isEnabled();
    }
  }
  FAIL("no inspector row " << row.toStdString());
  return false;
}
}

TEST_CASE("a texture inlet is read-only when the shader declares its sampler", "[gfx][isf][sampler][U34]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("two-inputs.fs");
    {
      QFile f{path};
      REQUIRE(f.open(QIODevice::WriteOnly));
      f.write(R"_(/*{ "ISFVSN": "2", "INPUTS": [
  { "NAME": "declared", "TYPE": "image", "WRAP": "clamp_to_edge" },
  { "NAME": "plain", "TYPE": "image" } ] }*/
void main() { gl_FragColor = IMG_NORM_PIXEL(declared, isf_FragNormCoord) + IMG_NORM_PIXEL(plain, isf_FragNormCoord); }
)_");
    }
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
    auto* fac = pl.get(Metadata<ConcreteKey_k, Gfx::Filter::Model>::get());
    REQUIRE(fac);
    auto* proc = fac->make(
        TimeVal::fromMsecs(1000.), path, Id<Process::ProcessModel>{1}, doc->context(),
        new QObject{&doc->model()});
    REQUIRE(proc);

    const Gfx::TextureInlet* declared{};
    const Gfx::TextureInlet* plain{};
    for(auto* in : proc->inlets())
      if(auto* t = qobject_cast<Gfx::TextureInlet*>(in))
        (t->name() == "declared" ? declared : plain) = t;
    REQUIRE(declared);
    REQUIRE(plain);
    CHECK(declared->samplerFromShader());
    CHECK_FALSE(plain->samplerFromShader());

    for(const QString row : {"Filter", "Address mode", "Mipmaps"})
    {
      CAPTURE(row.toStdString());
      CHECK_FALSE(rowEnabled(ctx, doc->context(), *declared, row));
      CHECK(rowEnabled(ctx, doc->context(), *plain, row));
    }
  });
}
