// The window score embeds an LV2 X11 UI in takes the size of the UI: the one
// the UI made itself when it opens, and the one it gives itself later without
// asking the host (DPF's resize handle, as Cardinal's). The fixture UI
// (tests/fixtures/lv2/score_test_x11ui.c) is 300 x 200, and 400 x 250 once
// its "size" control is over 0.5.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/ProcessList.hpp>

#include <Effect/EffectLayer.hpp>
#include <LV2/ApplicationPlugin.hpp>
#include <LV2/Context.hpp>
#include <LV2/EffectModel.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QWidget>

#include <catch2/catch_test_macros.hpp>

namespace
{
void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QApplication::processEvents();
}

//! Processes events until `cond` holds, for at most `ms`: whether it holds
template <typename F>
bool spin_until(F cond, int ms)
{
  QElapsedTimer t;
  t.start();
  while(!cond())
  {
    if(t.elapsed() > ms)
      return false;
    QApplication::processEvents();
  }
  return true;
}

QString lv2_bundle()
{
  return QString::fromUtf8(SCORE_TEST_LV2_BUNDLE);
}

LV2::Model& add_x11ui(const score::GUIApplicationContext& ctx, score::Document& doc)
{
  auto& lv2 = ctx.applicationPlugin<LV2::ApplicationPlugin>();
  lv2.lv2_context->loadPlugins();
  LV2::PluginInfo info;
  info.bundle = lv2_bundle();
  info.uri = "urn:score:test:x11ui";
  info.name = "Score Test X11 UI";
  info.valid = true;
  lv2.setCachedDescriptors({info});
  // Not made when plug-ins are disabled, as in the tests
  if(!lv2.lv2_context->ui_host)
    lv2.lv2_context->ui_host
        = lv2.suil.host_new(LV2::on_uiMessage, LV2::port_index, nullptr, nullptr);

  auto& interval = score::test::base_interval(doc);
  auto* cmd = new Scenario::Command::AddOnlyProcessToInterval{
      interval, Metadata<ConcreteKey_k, LV2::Model>::get(),
      QStringLiteral("urn:score:test:x11ui"), QPointF{}};
  CommandDispatcher<>{doc.context().commandStack}.submit(cmd);
  auto it = interval.processes.find(cmd->processId());
  REQUIRE(it != interval.processes.end());
  auto* proc = qobject_cast<LV2::Model*>(&*it);
  REQUIRE(proc);
  REQUIRE(proc->plugin);
  return *proc;
}
}

TEST_CASE(
    "LV2: the window of an X11 UI takes the size the UI gives itself",
    "[integration][lv2][gui]")
{
  qputenv("LV2_PATH", lv2_bundle().toUtf8());
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    if(QGuiApplication::platformName() != QLatin1String("xcb"))
      SKIP("X11 UIs are embedded on X11 only");

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = add_x11ui(ctx, *doc);

    Process::setupExternalUI(proc, doc->context(), true);
    QWidget* win = nullptr;
    // The size of the UI, not Qt's default for a window never shown
    CHECK(spin_until(
        [&] {
      win = proc.externalUI;
      return win && win->size() == QSize(300, 200);
    }, 5000));
    REQUIRE(win);

    // The UI resizes its own window, as the resize handle of DPF does
    Process::ControlInlet* size = nullptr;
    for(auto* inlet : proc.inlets())
      if(auto* c = qobject_cast<Process::ControlInlet*>(inlet))
        size = c;
    REQUIRE(size);
    size->setValue(1.f);
    CHECK(spin_until([&] { return win->size() == QSize(400, 250); }, 5000));

    // A resize from outside still reaches the UI, and is not undone
    win->resize(500, 300);
    spin(500);
    CHECK(win->size() == QSize(500, 300));

    Process::setupExternalUI(proc, doc->context(), false);
    spin(100);
  });
}
