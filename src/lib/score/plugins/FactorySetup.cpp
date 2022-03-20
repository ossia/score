#include <score/plugins/FactorySetup.hpp>
#include <score/application/GUIApplicationContext.hpp>

bool appcontext_has_ui(const score::GUIApplicationContext& ctx) noexcept
{
  return ctx.applicationSettings.gui;
}
bool appcontext_has_ui(const score::ApplicationContext& ctx) noexcept
{
    return ctx.applicationSettings.gui;
}
