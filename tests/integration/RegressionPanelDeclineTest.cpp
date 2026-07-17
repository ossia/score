// Regression test for db8477a5b — "core: don't store a null panel when a
// PanelDelegateFactory declines".
//
// A PanelDelegateFactory may return nullptr from make() (missing hardware,
// headless environment). GUIApplicationRegistrar::registerPanel dereferenced
// the result unconditionally (panel->setModel) and pushed the null unique_ptr
// into ApplicationComponents::panels, crashing both immediately and in every
// later panels() iteration. The fix warns and skips the factory.
//
// Without the fix this test dies with SIGSEGV inside registerPanel; with it,
// the panel list is left untouched.

#include <score_test/App.hpp>

#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>

#include <core/application/ApplicationRegistrar.hpp>
#include <core/application/MinimalApplication.hpp>
#include <core/presenter/Presenter.hpp>

#include <QApplication>

#include <catch2/catch_test_macros.hpp>

namespace
{
// A factory that declines: make() returns nullptr.
class DecliningPanelFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("aebb99e0-9785-4377-a449-e53e0dda2c38")

  std::unique_ptr<score::PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override
  {
    return nullptr;
  }
};
}

TEST_CASE(
    "A PanelDelegateFactory returning nullptr does not register a null panel",
    "[integration][regression][panels]")
{
  score::test::prepare_test_environment(/*headless=*/true);
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  score::MinimalApplication app;
  QApplication::processEvents();

  auto& components = app.componentsData();
  auto& presenter = *app.m_presenter;

  score::GUIApplicationRegistrar registrar{
      components, app.context(), presenter.menuManager(), presenter.toolbarManager(),
      presenter.actionManager()};

  const std::size_t panels_before = components.panels.size();

  DecliningPanelFactory factory;
  // Without the fix: null deref (panel->setModel) right here.
  registrar.registerPanel(factory);

  // With the fix: nothing was added...
  CHECK(components.panels.size() == panels_before);
  // ...and in particular no null slot that would crash later iterations.
  for(auto& panel : components.panels)
    CHECK(panel != nullptr);

  score::test::close_all_documents(app.context());
  QApplication::processEvents();
}
