// Regression test for f0d3ad966 — "core: also guard the loadPluginData
// panel-instantiation path against null".
//
// GUIApplicationInterface::registerPlugin instantiates every PanelDelegate-
// Factory a plugin exposes. A factory returning nullptr (declining) had its
// result dereferenced (p->setModel) and pushed into the panels list — the same
// defect as registerPanel, on the plugin-loading path. The fix warns and
// continues.
//
// We feed a fake plugin whose only GUI factory is a declining
// PanelDelegateFactory through the real registerPlugin path. Without the fix
// this dies with SIGSEGV inside registerPlugin; with it, no null panel is
// stored. The panel-instantiation loop only runs when the presenter has a
// view, so this uses the GUI application (hidden window; needs a display).

#include <score_test/App.hpp>

#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <core/application/ApplicationInterface.hpp>
#include <core/application/MinimalApplication.hpp>

#include <QApplication>

#include <catch2/catch_test_macros.hpp>

namespace
{
// A panel factory that declines: make() returns nullptr.
class DecliningPanelFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("786eebf1-6bd0-47a7-b532-936eeabf079e")

  std::unique_ptr<score::PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override
  {
    return nullptr;
  }
};

// A minimal plugin exposing only that factory.
class FakePanelPlugin final
    : public score::Plugin_QtInterface
    , public score::FactoryInterface_QtInterface
{
public:
  score::PluginKey key() const override
  {
    return score::PluginKey::fromString(
        QStringLiteral("4dc0322f-52d2-4856-8059-20f3369f7f69"));
  }

  std::vector<score::InterfaceBase*> guiFactories(
      const score::GUIApplicationContext& ctx,
      const score::InterfaceKey& factory_key) const override
  {
    if(factory_key == score::PanelDelegateFactory::static_interfaceKey())
      return {new DecliningPanelFactory};
    return {};
  }
};
}

TEST_CASE(
    "A declining panel factory loaded through registerPlugin does not store a "
    "null panel",
    "[integration][regression][panels][gui]")
{
  score::test::prepare_test_environment(/*headless=*/false);
  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  static int argc = 1;
  static char arg0[] = "score-test";
  static char* argv[] = {arg0, nullptr};
  score::MinimalGUIApplication app{argc, argv, /*show=*/false};
  QApplication::processEvents();

  auto& components = app.componentsData();
  const std::size_t panels_before = components.panels.size();

  FakePanelPlugin plugin;
  // Without the fix: null deref (p->setModel) inside registerPlugin.
  app.registerPlugin(plugin);

  // With the fix: the declining factory added no panel...
  CHECK(components.panels.size() == panels_before);
  // ...and every stored panel is valid (no null slot for later iterations).
  for(auto& panel : components.panels)
    CHECK(panel != nullptr);

  score::test::close_all_documents(app.context());
  QApplication::processEvents();
}
