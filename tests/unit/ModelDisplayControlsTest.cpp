// Model Display controls. The "Far" control starts inside its own range
// (shader board M04). It used to default to 100000 on a slider that stopped at 10000, and the
// render node itself started at 10000: the first frame and the UI disagreed.
// The default is 1000 (Unity, three.js and Godot are 1000 to 4000), the range
// reaches 100000 since score uses reverse-Z.
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/TimeValue.hpp>

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Threedim/ModelDisplay/Metadata.hpp>
#include <Threedim/ModelDisplay/ModelDisplayNode.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <catch2/catch_test_macros.hpp>

#include <ossia/network/value/value_conversion.hpp>

namespace
{
Process::ProcessModel* makeModelDisplay(
    const score::GUIApplicationContext& ctx, const score::Document& doc, QObject* owner)
{
  auto& pl = ctx.interfaces<Process::ProcessFactoryList>();
  auto* fac = pl.get(Metadata<ConcreteKey_k, Gfx::ModelDisplay::Model>::get());
  REQUIRE(fac);
  return fac->make(
      TimeVal::fromMsecs(1000.), {}, Id<Process::ProcessModel>{1}, doc.context(), owner);
}
}

TEST_CASE("Model Display Far starts inside its range", "[threedim][model-display][M04]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};
    auto* proc = makeModelDisplay(ctx, *doc, owner);
    REQUIRE(proc);

    Process::FloatSlider* far = nullptr;
    for(auto* in : proc->inlets())
      if(in->name() == "Far")
        far = qobject_cast<Process::FloatSlider*>(in);
    REQUIRE(far);

    const float def = ossia::convert<float>(far->init());
    CHECK(def == 1000.f);
    CHECK(far->getMax() >= 100000.f);
    CHECK(def <= far->getMax());
    CHECK(def >= far->getMin());

    // The render node's first frame uses the same value.
    const score::gfx::ModelDisplayNode node;
    CHECK(node.far == def);
  });
}

// The blend controls are "Op Color" / "Op Alpha" and spell "Subtract" (M08).
TEST_CASE("Model Display blend labels", "[threedim][model-display][M08]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    QObject* owner = new QObject{&doc->model()};
    auto* proc = makeModelDisplay(ctx, *doc, owner);
    REQUIRE(proc);

    QStringList names;
    for(auto* in : proc->inlets())
      names.push_back(in->name());
    CHECK(names.contains("Op Color"));
    CHECK(names.contains("Op Alpha"));
    CHECK_FALSE(names.contains("Op alpha"));

    for(auto* in : proc->inlets())
    {
      if(!in->name().startsWith("Op "))
        continue;
      auto* combo = qobject_cast<Process::ComboBox*>(in);
      REQUIRE(combo);
      QStringList labels;
      for(const auto& [label, value] : combo->alternatives)
        labels.push_back(label);
      CAPTURE(labels.join(", ").toStdString());
      CHECK(labels.contains("Subtract"));
      CHECK(labels.contains("Reverse Subtract"));
      CHECK_FALSE(labels.join(" ").contains("Substract"));
    }
  });
}
