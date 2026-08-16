#include <LV2/ApplicationPlugin.hpp>
#include <LV2/Settings.hpp>
#include <Media/Effect/Settings/Model.hpp>
#include <Media/Effect/Settings/PluginTab.hpp>

#include <score/application/GUIApplicationContext.hpp>

namespace LV2
{
QString SettingsWidget::name() const noexcept
{
  return "LV2";
}

QWidget* SettingsWidget::make(const score::ApplicationContext& ctx)
{
  auto& model = ctx.settings<Media::Settings::Model>();
  auto& gctx = static_cast<const score::GUIApplicationContext&>(ctx);
  auto& plug = gctx.applicationPlugin<LV2::ApplicationPlugin>();

  Media::Settings::PluginTabSpec spec;
  spec.pathsLabel = QObject::tr("LV2 paths");
  spec.getPaths = [&model] { return model.getLv2Paths(); };
  spec.commitPaths = [this, &model](QStringList paths) {
    m_disp.submit<Media::Settings::SetModelLv2Paths>(model, std::move(paths));
  };
  spec.onPathsChanged = [&model](QObject* c, std::function<void(QStringList)> f) {
    QObject::connect(
        &model, &Media::Settings::Model::Lv2PathsChanged, c, std::move(f));
  };
  spec.rows = [&plug] {
    std::vector<Media::Settings::PluginTabRow> rows;
    const auto& descs = plug.cachedDescriptors();
    rows.reserve(descs.size());
    for(const auto& p : descs)
      rows.push_back({p.name, p.bundle, p.valid});
    return rows;
  };
  spec.onPluginsChanged = [&plug](QObject* c, std::function<void()> f) {
    QObject::connect(
        &plug, &LV2::ApplicationPlugin::descriptorsChanged, c, std::move(f));
  };
  spec.rescan = [&plug] { plug.forceRescan(); };

  return Media::Settings::makePluginSettingsWidget(std::move(spec));
}
}
