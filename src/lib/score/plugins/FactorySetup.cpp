#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/FactorySetup.hpp>

bool appcontext_has_ui(const score::GUIApplicationContext& ctx) noexcept
{
  return ctx.applicationSettings.gui;
}
bool appcontext_has_ui(const score::ApplicationContext& ctx) noexcept
{
  return ctx.applicationSettings.gui;
}
