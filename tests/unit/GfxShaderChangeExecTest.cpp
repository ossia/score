// A shader process whose program is replaced while the document plays, then
// put back by undo. Restoring the ports' saved data (here an exposed name and
// the scriptable flag) has the execution setup rebind their addresses: those
// rebinds must not reach an inlet the program change has replaced.

#include <Gfx/Filter/Process.hpp>

#include <Process/Dataflow/Port.hpp>

#include <Execution/DocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <QApplication>
#include <QFile>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Execution.hpp>
#include <score_test/Project.hpp>

namespace
{
const QString filter_uuid = QStringLiteral("74ca45ff-92c9-44a0-8f1a-754dea05ee1b");

const char* checkerboard = R"(/*{
  "INPUTS": [
    { "NAME": "width", "TYPE": "float", "DEFAULT": 0.25, "MIN": 0.0, "MAX": 1.0 },
    { "NAME": "offset", "TYPE": "point2D", "DEFAULT": [0.0, 0.0] }
  ]
}*/
void main() {
  vec2 c = floor((isf_FragNormCoord + offset) / max(width, 0.01));
  gl_FragColor = vec4(vec3(mod(c.x + c.y, 2.0)), 1.0);
})";

const char* waves = R"(/*{
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image" },
    { "NAME": "distortionAmount", "TYPE": "float", "DEFAULT": 0.5, "MIN": 0.0, "MAX": 2.0 },
    { "NAME": "waveSpeed", "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.0, "MAX": 10.0 }
  ]
}*/
void main() {
  vec2 uv = isf_FragNormCoord;
  uv.x += sin(uv.y * 5.0 + TIME * waveSpeed) * distortionAmount * 0.05;
  gl_FragColor = IMG_NORM_PIXEL(inputImage, uv);
})";

void settle(Execution::DocumentPlugin& plug)
{
  for(int i = 0; i < 4; i++)
  {
    score::test::run_exec(plug);
    QApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  }
  score::test::run_exec(plug);
}
}

TEST_CASE(
    "A shader replaced and put back while playing leaves no stale inlet behind",
    "[gfx][execution][ports]")
{
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(*doc, filter_uuid, {});
    if(!proc)
      SKIP("not built");
    auto& model = static_cast<Gfx::Filter::Model&>(*proc);
    (void)model.setProgram(Gfx::ShaderSource{
        Gfx::ShaderSource::ProgramType::ISF, QString{}, QString::fromUtf8(checkerboard)});
    model.programChanged();
    REQUIRE(model.inlets().size() >= 2);

    auto& plug = doc->context().plugin<Execution::DocumentPlugin>();
    plug.reload(true, score::test::base_interval(*doc));
    settle(plug);

    // Saved data whose restoring emits the signals the setup rebinds on
    auto* width = model.inlets()[0];
    width->setScriptable(true);
    width->setExposed(QStringLiteral("cell_width"));
    settle(plug);

    CommandDispatcher<> disp{doc->context().commandStack};
    for(int round = 0; round < 3; round++)
    {
      disp.submit(new Gfx::ChangeShader{
          model,
          Gfx::ShaderSource{
              Gfx::ShaderSource::ProgramType::ISF, QString{}, QString::fromUtf8(waves)},
          doc->context()});
      settle(plug);
      doc->commandStack().undo();
      settle(plug);
    }
    CHECK(model.inlets().size() >= 2);
  });
}
