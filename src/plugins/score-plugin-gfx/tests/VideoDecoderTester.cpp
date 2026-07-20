// Standalone test harness: like VideoTester, but usable headless.
// - Marks the main thread as Ui so GfxContext's ensure_current_thread(Ui)
//   assertion passes under MinimalGUIApplication (VideoTester aborts there).
// - Exit codes: 0 = played and closed cleanly, 4 = drop produced no
//   Gfx::TextureOutlet (file not recognized as a video process).
// - SCORE_VIDEO_TEST_MS overrides the 4000 ms play duration.
//
// Decode-correctness mode (tests/integration/video-decode-correctness.sh):
//   VideoDecoderTester <clip> --expect <ref.png> [--psnr <dB>] [--grab <out.png>]
// forces the "window" device offscreen (SCORE_FORCE_OFFSCREEN_WINDOW), sizes
// the render target to the reference image, polls the offscreen readback
// during playback and exits 0 as soon as the rendered RGBA matches the
// reference within the PSNR threshold. Exit 5 = never matched (best PSNR is
// printed, and the last frame is dumped next to --grab / the ref for triage).
// --grab alone (no --expect) saves the first non-blank frame and exits 0 —
// used to inspect the harness output / bootstrap references.
#include "score_integration.hpp"

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
#include <Gfx/Settings/Model.hpp>
#include <Gfx/TexturePort.hpp>
#include <Gfx/WindowDevice.hpp>
#include <Gfx/Window/OffscreenDevice.hpp>

#include <QImage>

#include <cmath>

#include <Scenario/Application/Drops/AutomationDropHandler.cpp>
#include <Scenario/Commands/Cohesion/CreateCurves.cpp>

static int videoTestDurationMs()
{
  bool ok = false;
  int ms = qEnvironmentVariableIntValue("SCORE_VIDEO_TEST_MS", &ok);
  return (ok && ms > 0) ? ms : 4000;
}

// ---- decode-correctness mode --------------------------------------------
struct TesterOptions
{
  QStringList files;
  QString expect;    // reference PNG; enables compare mode
  QString grab;      // where to save the (first non-blank / failing) frame
  double psnr = 30.; // pass threshold in dB
  bool compareMode() const { return !expect.isEmpty() || !grab.isEmpty(); }
};

static TesterOptions parseOptions(QStringList args)
{
  TesterOptions o;
  args.pop_front(); // binary name
  for(int i = 0; i < args.size(); i++)
  {
    const QString& a = args[i];
    if(a == "--expect" && i + 1 < args.size())
      o.expect = args[++i];
    else if(a == "--grab" && i + 1 < args.size())
      o.grab = args[++i];
    else if(a == "--psnr" && i + 1 < args.size())
      o.psnr = args[++i].toDouble();
    else if(!a.startsWith("--"))
      o.files.push_back(a);
  }
  return o;
}

// Mean of the RGB channels in [0; 255]; used to reject the initial black.
static double meanRGB(const QImage& img)
{
  double sum = 0.;
  const int W = img.width(), H = img.height();
  if(W <= 0 || H <= 0)
    return 0.;
  for(int y = 0; y < H; y++)
  {
    const uchar* p = img.constScanLine(y);
    for(int x = 0; x < W * 4; x += 4)
      sum += p[x] + p[x + 1] + p[x + 2];
  }
  return sum / (double(W) * H * 3.);
}

// PSNR over the RGB channels (alpha excluded: alpha-format clips are kept
// fully opaque, the output compositing path would make blended alpha
// unpredictable to reference).
static double computePSNR(const QImage& ref0, const QImage& img0)
{
  QImage a = ref0.convertToFormat(QImage::Format_RGBA8888);
  QImage b = img0.convertToFormat(QImage::Format_RGBA8888);
  if(a.isNull() || b.isNull())
    return 0.;
  if(b.size() != a.size())
    b = b.scaled(a.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  double mse = 0.;
  const int W = a.width(), H = a.height();
  for(int y = 0; y < H; y++)
  {
    const uchar* pa = a.constScanLine(y);
    const uchar* pb = b.constScanLine(y);
    for(int x = 0; x < W * 4; x += 4)
      for(int c = 0; c < 3; c++)
      {
        const double d = double(pa[x + c]) - double(pb[x + c]);
        mse += d * d;
      }
  }
  mse /= double(W) * H * 3.;
  if(mse <= 1e-9)
    return 99.;
  return 10. * std::log10(255. * 255. / mse);
}

// Current offscreen frame of the forced-offscreen "window" device
// (WindowDevice privates are visible through the #define private public
// above; same readback WindowDevice::grabTo serializes to disk).
static QImage currentOffscreenFrame(Gfx::WindowDevice& wd)
{
  auto od = dynamic_cast<Gfx::offscreen_device*>(wd.m_dev.get());
  if(!od)
    return {};
  auto node = od->node();
  if(!node || !node->shared_readback)
    return {};
  const auto& rb = *node->shared_readback;
  const int w = rb.pixelSize.width(), h = rb.pixelSize.height();
  if(w <= 0 || h <= 0 || rb.data.size() < w * h * 4)
    return {};
  const QImage img{
      reinterpret_cast<const uchar*>(rb.data.constData()), w, h, w * 4,
      QImage::Format_RGBA8888};
  return img.copy();
}

// Leave through std::exit() from inside the event loop: the atexit chain
// runs, so every instrumented DSO's profile runtime flushes its .profraw
// (LLVM_PROFILE_FILE must contain %m so they don't clobber one another),
// while the app/document destructors in main() — which currently crash, see
// the notes at the exit timer below — are skipped entirely.
static void flushCoverageAndExit(int code)
{
  ::fflush(nullptr);
  // Under llvmpipe+ASAN the atexit chain SIGABRTs in a llvmpipe worker
  // thread after our verdict is printed, clobbering the exit code the
  // decode-correctness driver relies on. Only walk atexit (std::exit) when a
  // coverage run actually needs the profile runtimes flushed.
  if(qEnvironmentVariableIsSet("LLVM_PROFILE_FILE"))
    std::exit(code);
  ::_exit(code);
}


void VideoTest()
{
  const auto& ctx = score::GUIAppContext();

  // Compare mode runs on the offscreen QPA: the QOpenGLWidget viewport the
  // scenario view creates when applicationSettings.opengl is set (the
  // MinimalGUIApplication never runs the CLI parse, so it stays at the
  // header default `true`) makes QWidgetRepaintManager::flush go through
  // QPlatformBackingStore::rhiFlush, which SIGSEGVs on the offscreen
  // platform. Mirror the full app's --no-opengl before the view is built.
  if(qEnvironmentVariable("SCORE_FORCE_OFFSCREEN_WINDOW") == "window")
    const_cast<score::ApplicationSettings&>(ctx.applicationSettings).opengl = false;

  // SCORE_TEST_API=vulkan|opengl overrides the render graph's graphics API
  // (same knob as EncoderMatrixTest). The offscreen QPA cannot create GL
  // contexts without a DISPLAY, so headless CI runs pin vulkan + lavapipe
  // (VK_LOADER_DRIVERS_SELECT=lvp*) instead.
  if(const auto api = qEnvironmentVariable("SCORE_TEST_API"); !api.isEmpty())
  {
    auto& gfxSettings
        = const_cast<Gfx::Settings::Model&>(ctx.settings<Gfx::Settings::Model>());
    if(api.compare("vulkan", Qt::CaseInsensitive) == 0)
      gfxSettings.setGraphicsApi(Gfx::Settings::GraphicsApis{}.Vulkan);
    else if(api.compare("opengl", Qt::CaseInsensitive) == 0)
      gfxSettings.setGraphicsApi(Gfx::Settings::GraphicsApis{}.OpenGL);
  }

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

  const TesterOptions opts = parseOptions(qApp->arguments());

  int outlets = 0;
  {
    QMimeData mime;
    QList<QUrl> url;
    for(auto file : opts.files)
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

  // Decode-correctness mode: locate the (offscreen-forced) window device,
  // size its render target to the reference so the comparison is 1:1.
  Gfx::WindowDevice* windowDevice = nullptr;
  auto expected = std::make_shared<QImage>();
  if(opts.compareMode())
  {
    auto& device_plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    windowDevice
        = qobject_cast<Gfx::WindowDevice*>(device_plug.list().findDevice("window"));
    if(!windowDevice
       || !dynamic_cast<Gfx::offscreen_device*>(windowDevice->m_dev.get()))
    {
      fprintf(
          stderr, "VideoDecoderTester: --expect/--grab need the offscreen window "
                  "device (SCORE_FORCE_OFFSCREEN_WINDOW=window)\n");
      flushCoverageAndExit(6);
    }

    if(!opts.expect.isEmpty())
    {
      *expected = QImage{opts.expect}.convertToFormat(QImage::Format_RGBA8888);
      if(expected->isNull())
      {
        fprintf(
            stderr, "VideoDecoderTester: cannot load reference %s\n",
            opts.expect.toUtf8().constData());
        flushCoverageAndExit(6);
      }
      auto od = static_cast<Gfx::offscreen_device*>(windowDevice->m_dev.get());
      od->node()->setSize(QSize{expected->width(), expected->height()});
    }
  }

  // Start execution
  auto& eng = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
  QTimer::singleShot(100, [&] { eng.execution().play_interval(root_itv); });

  if(opts.compareMode())
  {
    // Poll the offscreen readback until it matches (or the deadline hits).
    auto poll = new QTimer(qApp);
    auto best = std::make_shared<double>(-1.);
    auto bestFrame = std::make_shared<QImage>();
    poll->setInterval(200);
    QObject::connect(poll, &QTimer::timeout, qApp, [=] {
      if(qEnvironmentVariableIntValue("SCORE_VIDEO_TEST_DEBUG") == 1)
      {
        auto od = dynamic_cast<Gfx::offscreen_device*>(windowDevice->m_dev.get());
        auto node = od ? od->node() : nullptr;
        auto rl = node ? node->renderer() : nullptr;
        fprintf(
            stderr, "poll: node=%p renderlist=%p renderers=%zu readback=%dx%d (%d bytes)\n",
            (void*)node, (void*)rl, rl ? rl->renderers.size() : size_t(0),
            node && node->shared_readback ? node->shared_readback->pixelSize.width() : -1,
            node && node->shared_readback ? node->shared_readback->pixelSize.height() : -1,
            node && node->shared_readback ? int(node->shared_readback->data.size()) : -1);
      }
      QImage frame = currentOffscreenFrame(*windowDevice);
      if(qEnvironmentVariableIntValue("SCORE_VIDEO_TEST_DEBUG") == 1 && !frame.isNull())
        fprintf(stderr, "poll: frame meanRGB=%.3f\n", meanRGB(frame));
      if(frame.isNull() || meanRGB(frame) < 2.)
        return; // still black / not rendering yet
      if(opts.expect.isEmpty())
      {
        // --grab only: first non-blank frame wins.
        frame.save(opts.grab);
        printf("GRABBED %s\n", opts.grab.toUtf8().constData());
        flushCoverageAndExit(0);
      }
      const double p = computePSNR(*expected, frame);
      if(p > *best)
      {
        *best = p;
        *bestFrame = frame;
      }
      if(p >= opts.psnr)
      {
        if(!opts.grab.isEmpty())
          frame.save(opts.grab);
        printf("DECODE-PSNR %.2f dB (threshold %.2f) PASS\n", p, opts.psnr);
        flushCoverageAndExit(0);
      }
    });
    poll->start();

    QTimer::singleShot(videoTestDurationMs(), [=] {
      QString dump = !opts.grab.isEmpty() ? opts.grab : opts.expect + ".actual.png";
      if(!bestFrame->isNull())
        bestFrame->save(dump);
      printf(
          "DECODE-PSNR %.2f dB (threshold %.2f) FAIL actual=%s\n",
          *best, opts.psnr, dump.toUtf8().constData());
      flushCoverageAndExit(5);
    });
    return;
  }

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

  // Decode-correctness mode reads the offscreen readback of the "window"
  // device: force it offscreen before the app plugins come up.
  for(int i = 1; i < argc; i++)
    if(!strcmp(argv[i], "--expect") || !strcmp(argv[i], "--grab"))
    {
      qputenv("SCORE_FORCE_OFFSCREEN_WINDOW", "window");
      break;
    }

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
