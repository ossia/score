// Panning a sound layer must keep a waveform under the viewport.
//
// The image covers only the span it was asked for, so every way the view can
// move has to end in a new request. The timeline pans its content rather than
// moving a scrollbar, and the scrollbar signal the layer hangs its recompute
// on then never fires: what is on screen leaves the rendered window and the
// layer goes blank until something unrelated happens to ask again.

#include <Media/Sound/SoundModel.hpp>
#include <Media/Sound/SoundView.hpp>
#include <Process/ZoomHelper.hpp>

#include <ossia/detail/flicks.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QTemporaryDir>

#include <catch2/catch_all.hpp>

using namespace score::test;

namespace
{
constexpr double file_seconds = 180.;
constexpr double layer_width = 8000.;

void pump(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QApplication::processEvents(QEventLoop::AllEvents, 5);
}
} // namespace

TEST_CASE(
    "A panned sound layer keeps a waveform under the view", "[integration][waveform]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir mediaDir;
    REQUIRE(mediaDir.isValid());
    const QString wav = mediaDir.path() + "/long.wav";
    write_wav(wav, file_seconds);

    auto* doc = new_document(ctx);
    REQUIRE(doc);
    auto* sound = qobject_cast<Media::Sound::ProcessModel*>(
        add_process(*doc, sound_process_uuid, wav));
    REQUIRE(sound != nullptr);

    // A view narrower than the layer, so there is something to pan through.
    QGraphicsScene scene;
    QGraphicsView view{&scene};
    view.resize(800, 300);
    view.show();
    pump(200);

    // LayerView's constructor reaches through its parent to find the view.
    auto* parent = new QGraphicsRectItem{};
    scene.addItem(parent);
    auto* layer = new Media::Sound::LayerView{*sound, parent};
    layer->setPos(0, 0);
    layer->setHeight(200.);
    layer->setWidth(layer_width);
    layer->setData(sound->file());
    layer->recompute(
        sound->duration().impl / double(ossia::flicks_per_second<double>) * 1000.
        / layer_width);
    scene.setSceneRect(0, 0, layer_width, 300);
    pump(1500);

    const auto visible_span = [&] {
      const double x0 = layer->mapFromScene(view.mapToScene(0, 0)).x();
      const double xf
          = layer->mapFromScene(view.mapToScene(view.viewport()->width(), 0)).x();
      return QPair<double, double>{std::max(0., x0), std::min(layer_width, xf)};
    };

    int uncovered = 0, checked = 0;
    for(int step = 0; step < 12; step++)
    {
      // Panned, not scrolled: no scrollbar signal is emitted at all.
      parent->setPos(-step * 600., 0.);
      pump(400);

      const auto [vx0, vxf] = visible_span();
      if(vxf - vx0 < 10.)
        continue;
      ++checked;

      const QRectF span = layer->renderedSpan();
      INFO(
          "step " << step << ": visible [" << vx0 << ", " << vxf << "], rendered ["
                  << span.left() << ", " << span.right() << "]");
      if(span.isEmpty() || span.left() > vx0 + 4. || span.right() < vxf - 4.)
        ++uncovered;
    }

    REQUIRE(checked > 6);
    CHECK(uncovered == 0);

    delete parent;
  });
}
