// Integration test: a looping clip in the nodal view is drawn by its loop.
//
// A node draws its process over the node's width: a whole loop when the
// process loops, the whole process otherwise. A looper's take dropped on a
// box longer than it, in the nodal view, loops at the take's length: its
// piano roll must show the take over the node, not the take squeezed into the
// first part of the box's length.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Drop/ClipMetadata.hpp>
#include <Process/ProcessMimeSerialization.hpp>

#include <Scenario/Document/Interval/FullView/NodalIntervalView.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <Midi/MidiProcess.hpp>
#include <Midi/MidiView.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QMimeData>
#include <QTemporaryDir>
#include <QUrl>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <libremidi/writer.hpp>

#include <sstream>

using Catch::Approx;

namespace
{
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  do
  {
    QApplication::processEvents(QEventLoop::AllEvents, 5);
  } while(t.elapsed() < ms);
}

Scenario::NodalIntervalView* nodalView(QGraphicsScene& scene)
{
  for(auto item : scene.items())
    if(auto n = dynamic_cast<Scenario::NodalIntervalView*>(item))
      return n;
  return nullptr;
}

Midi::View* midiView(QGraphicsScene& scene)
{
  for(auto item : scene.items())
    if(auto v = dynamic_cast<Midi::View*>(item))
      return v;
  return nullptr;
}

//! Four bars of quarters in 4/4 at 120 bpm: 8 s.
QByteArray four_bars()
{
  libremidi::writer w;
  w.ticksPerQuarterNote = 480;
  w.add_event(0, 0, libremidi::meta_events::tempo(500000));
  w.add_event(0, 0, libremidi::meta_events::time_signature(4, 4, 24, 8));
  for(int q = 0; q < 16; q++)
  {
    w.add_event(0, 0, libremidi::channel_events::note_on(1, 60 + q % 4, 100));
    w.add_event(480, 0, libremidi::channel_events::note_off(1, 60 + q % 4, 0));
  }
  std::ostringstream out;
  w.write(out);
  const auto s = std::move(out).str();
  return QByteArray(s.data(), qsizetype(s.size()));
}
}

TEST_CASE(
    "A take dropped in the nodal view is drawn by its loop, not by its box",
    "[integration][nodal][midi][looper][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);
    auto presenter
        = score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
            *doc);
    REQUIRE(presenter != nullptr);
    auto& itv = static_cast<Scenario::ScenarioDocumentModel&>(doc->model().modelDelegate())
                    .baseInterval();
    presenter->setNodalMode(true);
    spin(100);

    auto& scene = presenter->view().scene();
    auto nodal = nodalView(scene);
    REQUIRE(nodal != nullptr);

    QTemporaryDir dir;
    const QString path = dir.path() + "/Looper-take1.mid";
    {
      QFile f{path};
      REQUIRE(f.open(QIODevice::WriteOnly));
      f.write(four_bars());
    }
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(path)});
    const auto meta = Process::encode_clip_metadata(
        {path.toStdString()}, halp::clip_stretch::automatic, true);
    mime.setData(
        QString::fromUtf8(Process::clip_metadata_mime.data()),
        QByteArray(meta.data(), qsizetype(meta.size())));

    nodal->on_drop(QPointF{300., 300.}, &mime);
    spin(200);

    Midi::ProcessModel* midi{};
    for(auto& p : itv.processes)
      if(auto m = qobject_cast<Midi::ProcessModel*>(&p))
        midi = m;
    REQUIRE(midi != nullptr);
    REQUIRE(midi->loops());
    REQUIRE(midi->loopDuration().msec() == Approx(8000.).margin(1.));
    REQUIRE(midi->duration().msec() > midi->loopDuration().msec() + 1.);

    auto view = midiView(scene);
    // The node's width is one loop: the whole process is as many node widths
    // as it has loops.
    const double node_w = view->boundingRect().width();
    REQUIRE(node_w > 0.);
    CHECK(
        view->defaultWidth()
        == Approx(node_w * midi->duration().msec() / midi->loopDuration().msec())
               .epsilon(0.01));
  });
}
