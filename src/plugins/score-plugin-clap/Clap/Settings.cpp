#include <Clap/ApplicationPlugin.hpp>
#include <Clap/Settings.hpp>
#include <Media/Effect/Settings/Model.hpp>
#include <Media/Effect/Settings/PluginTab.hpp>

#include <score/application/GUIApplicationContext.hpp>

namespace Clap
{
QString SettingsWidget::name() const noexcept
{
  return "CLAP";
}

QWidget* SettingsWidget::make(const score::ApplicationContext& ctx)
{
  auto& model = ctx.settings<Media::Settings::Model>();
  auto& gctx = static_cast<const score::GUIApplicationContext&>(ctx);
  auto& plug = gctx.guiApplicationPlugin<Clap::ApplicationPlugin>();

  Media::Settings::PluginTabSpec spec;
  spec.pathsLabel = QObject::tr("CLAP paths");
  spec.getPaths = [&model] { return model.getClapPaths(); };
  spec.commitPaths = [this, &model](QStringList paths) {
    m_disp.submit<Media::Settings::SetModelClapPaths>(model, std::move(paths));
  };
  spec.onPathsChanged = [&model](QObject* c, std::function<void(QStringList)> f) {
    QObject::connect(
        &model, &Media::Settings::Model::ClapPathsChanged, c, std::move(f));
  };
  spec.rows = [&plug] {
    std::vector<Media::Settings::PluginTabRow> rows;
    rows.reserve(plug.plugins().size());
    for(const auto& p : plug.plugins())
      rows.push_back({p.name, p.path, p.valid});
    return rows;
  };
  spec.onPluginsChanged = [&plug](QObject* c, std::function<void()> f) {
    QObject::connect(
        &plug, &Clap::ApplicationPlugin::pluginsChanged, c, std::move(f));
  };
  spec.rescan = [&plug] { plug.forceRescan(); };

  return Media::Settings::makePluginSettingsWidget(std::move(spec));
}
}
