// Standalone test harness: like VideoTester, but usable headless.
// - Marks the main thread as Ui so GfxContext's ensure_current_thread(Ui)
//   assertion passes under MinimalGUIApplication (VideoTester aborts there).
// - Exit codes: 0 = played and closed cleanly, 4 = drop produced no
//   Gfx::TextureOutlet (file not recognized as a video process).
// - SCORE_VIDEO_TEST_MS overrides the 4000 ms play duration.
#include "../../../../tests/Integration/score_integration.hpp"

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/CoreApplicationPlugin.hpp>

#include <ossia/detail/thread.hpp>

#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

#include <wobjectimpl.h>

#include <verdigris>
#define private public
#include <Explorer/Commands/Add/LoadDevice.hpp>

#include <Scenario/Application/Drops/AutomationDropHandler.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <Gfx/TexturePort.hpp>
#include <Gfx/WindowDevice.hpp>

#include <Scenario/Application/Drops/AutomationDropHandler.cpp>
#include <Scenario/Commands/Cohesion/CreateCurves.cpp>

static int videoTestDurationMs()
{
  bool ok = false;
  int ms = qEnvironmentVariableIntValue("SCORE_VIDEO_TEST_MS", &ok);
  return (ok && ms > 0) ? ms : 4000;
}

// Leave through std::exit() from inside the event loop: the atexit chain
// runs, so every instrumented DSO's profile runtime flushes its .profraw
// (LLVM_PROFILE_FILE must contain %m so they don't clobber one another),
// while the app/document destructors in main() — which currently crash, see
// the notes at the exit timer below — are skipped entirely.
static void flushCoverageAndExit(int code)
{
  ::fflush(nullptr);
  std::exit(code);
}


void VideoTest()
{
  const auto& ctx = score::GUIAppContext();

  auto doc = ctx.docManager.newDocument(
      ctx, Id<score::DocumentModel>(0),
      *ctx.interfaces<score::DocumentDelegateList>().begin());

  qApp->processEvents();
  auto& doc_pm = doc->model().modelDelegate();

  // Create a window device
  {
    auto& device_plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    CommandDispatcher<> disp{doc->context().commandStack};
    Device::DeviceSettings settings;
    settings.name = "window";
    settings.protocol = Gfx::WindowProtocolFactory::static_concreteKey();

    disp.submit<Explorer::Command::LoadDevice>(device_plug, settings);
  }

  // Simulate a drop of video files
  auto& scenario_dm = static_cast<Scenario::ScenarioDocumentModel&>(doc_pm);
  auto& root_itv = scenario_dm.baseInterval();

  int outlets = 0;
  {
    QMimeData mime;
    QList<QUrl> url;
    auto args = qApp->arguments();
    args.pop_front(); // Remove the binary name
    for(auto file : args)
    {
      if(QFile f{file}; f.exists())
      {
        url.push_back(QUrl::fromLocalFile(QFileInfo{f}.absoluteFilePath()));
      }
    }
    mime.setUrls(url);

    Scenario::DropProcessInInterval drop;
    drop.drop(doc->context(), root_itv, QPointF{}, mime);

    for(auto& outlet : root_itv.findChildren<Gfx::TextureOutlet*>())
    {
      outlet->setAddress(State::AddressAccessor{{{"window"}, {}}});
      ++outlets;
    }
  }

  if(outlets == 0)
  {
    fprintf(stderr, "VideoDecoderTester: no TextureOutlet after drop\n");
    flushCoverageAndExit(4);
  }

  // Start execution
  auto& eng = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
  QTimer::singleShot(100, [&] { eng.execution().play_interval(root_itv); });
  // Stop execution. Default: flush coverage and leave without tearing the
  // app down. Known teardown crashes (both reproducible here, set
  // SCORE_VIDEO_TEST_TEARDOWN=1 to exercise them):
  // - forceCloseDocument: UAF, queued ScenarioDocumentPresenter::
  //   on_windowSizeChanged fires after the interval model is deleted
  //   (IntervalDurations::guiDuration()).
  // - ~MinimalGUIApplication: UAF, ~ScenarioDocumentPresenter ->
  //   DisplayedElementsPresenter::remove() -> interfaces<MagnetismAdjuster>()
  //   reads the already-destroyed ApplicationComponentsData map.
  QTimer::singleShot(videoTestDurationMs(), [&ctx, doc] {
    if(qEnvironmentVariableIntValue("SCORE_VIDEO_TEST_TEARDOWN") == 1)
    {
      ctx.docManager.forceCloseDocument(ctx, *doc);
      qApp->exit(0);
      return;
    }
    flushCoverageAndExit(0);
  });
}

int main(int argc, char** argv)
{
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  // The full application does this in ossia::context (libossia context.cpp);
  // without it, GfxContext's ensure_current_thread(thread_type::Ui) aborts.
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);

  score::MinimalGUIApplication app(argc, argv);

  QMetaObject::invokeMethod(
      &app,
      [] {
    VideoTest();
    QApplication::processEvents();
      },
      Qt::QueuedConnection);

  return app.exec();
}
