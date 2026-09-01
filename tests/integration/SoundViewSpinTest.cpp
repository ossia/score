// A waveform that cannot be computed must not be asked for again on every
// repaint. recompute() gives up without marking itself done when the file has
// no data, and paint_impl asked whenever there was nothing drawn: the two
// called each other for as long as the process existed, which pegs the thread
// that paints. In a browser that is the only thread, so the page dies -- and a
// terminal never has the file, since it is on the machine running the score.

#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Media/Sound/SoundView.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/document/DocumentContext.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <Process/ProcessList.hpp>

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QPainter>
#include <QPixmap>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("A waveform that cannot be drawn is not asked for again", "[media]")
{
  qputenv("SCORE_DISABLE_LIBRARY", "1");

  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto& itv = safe_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
                    .baseScenario()
                    .interval();

    // A path that names nothing here, which is what a terminal always has: the
    // file is on the machine running the score.
    doc->context().document.commandStack().redoAndPush(
        new Scenario::Command::AddOnlyProcessToInterval{
            itv, Metadata<ConcreteKey_k, Media::Sound::ProcessModel>::get(),
            QStringLiteral("/nonexistent/not-here.wav"), QPointF{}});

    Media::Sound::ProcessModel* model{};
    for(auto& p : itv.processes)
      if(auto* snd = qobject_cast<Media::Sound::ProcessModel*>(&p))
        model = snd;
    REQUIRE(model);
    REQUIRE(model->file());
    REQUIRE(model->file()->sampleRate() < 1);

    // In a scene, as a real layer is: the view reads the scene's view for the
    // device pixel ratio.
    QGraphicsScene scene;
    auto* root = new QGraphicsRectItem;
    scene.addItem(root);

    Media::Sound::LayerView view{*model, root};
    view.setWidth(400.);
    view.setHeight(100.);
    view.recompute(1.);
    view.setData(model->file());

    QPixmap pm{400, 100};
    QPainter p{&pm};

    const int before = view.recomputeCount();
    for(int i = 0; i < 50; i++)
      view.paint(&p, nullptr, nullptr);

    // Painting is the only proof that a view exists, which recompute() needs
    // and nothing else signals, so one ask is legitimate. What must be bounded
    // is the total: nothing about the answer changes between two paints, and
    // asking on each of them is a loop that outlives the process. When the data
    // does arrive, on_newData asks on its own.
    CHECK(view.recomputeCount() <= before + 1);
  });
}
