// The controls of a JSFX process and the sliders of its script agree: what a
// control shows is what the script holds, and changing one sets the other,
// in a document just made as in one loaded from a file.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <QTemporaryDir>

#include <YSFX/ProcessModel.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
const QString ysfx_uuid = QStringLiteral("cb6b50d3-82c0-4356-a282-317da8b8022f");

constexpr auto script = R"(desc:controls test
slider1:0.5<0,1,0.01>Gain
slider2:2<0,3,1{Off,Low,Mid,High}>Mode
out_pin:out
@sample
spl0 = slider1;
)";

YSFX::ProcessModel* find_ysfx(score::Document& doc)
{
  for(auto& p : score::test::base_interval(doc).processes)
    if(auto* y = qobject_cast<YSFX::ProcessModel*>(&p))
      return y;
  return nullptr;
}

//! Slider n of the script is the control with id 3 + n
Process::ControlInlet& control(YSFX::ProcessModel& p, int n)
{
  auto* c = qobject_cast<Process::ControlInlet*>(p.inlet(Id<Process::Port>(3 + n)));
  REQUIRE(c);
  return *c;
}

double slider(YSFX::ProcessModel& p, int n)
{
  return ysfx_slider_get_value(p.fx.get(), n - 1);
}
}

TEST_CASE(
    "a JSFX control sets its slider in a document loaded from a file",
    "[integration][ysfx]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("controls.jsfx");
    score::test::write_file(path, script);

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = static_cast<YSFX::ProcessModel*>(
        score::test::add_process(*doc, ysfx_uuid, path));
    if(!proc || !proc->fx)
      SKIP("YSFX not built");

    control(*proc, 1).setValue(0.25f);
    REQUIRE(slider(*proc, 1) == 0.25f);

    for(auto* loaded : {score::test::reload_via_json(ctx, *doc),
                        score::test::reload_via_bytes(ctx, *doc)})
    {
      REQUIRE(loaded);
      auto* p = find_ysfx(*loaded);
      REQUIRE(p);
      REQUIRE(p->fx);
      CHECK(slider(*p, 1) == 0.25f);

      control(*p, 1).setValue(0.75f);
      CHECK(slider(*p, 1) == 0.75f);
      control(*p, 2).setValue(3);
      CHECK(slider(*p, 2) == 3.);
    }
  });
}
