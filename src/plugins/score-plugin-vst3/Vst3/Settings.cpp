#include <Media/Effect/Settings/Model.hpp>
#include <Media/Effect/Settings/PluginTab.hpp>
#include <Vst3/ApplicationPlugin.hpp>
#include <Vst3/Settings.hpp>

#include <score/application/GUIApplicationContext.hpp>

namespace vst3
{
QString SettingsWidget::name() const noexcept
{
  return "VST3";
}

QWidget* SettingsWidget::make(const score::ApplicationContext& ctx)
{
  auto& model = ctx.settings<Media::Settings::Model>();
  auto& gctx = static_cast<const score::GUIApplicationContext&>(ctx);
  auto& plug = gctx.applicationPlugin<vst3::ApplicationPlugin>();

  Media::Settings::PluginTabSpec spec;
  spec.pathsLabel = QObject::tr("VST3 paths");
  spec.getPaths = [&model] { return model.getVst3Paths(); };
  spec.commitPaths = [this, &model](QStringList paths) {
    m_disp.submit<Media::Settings::SetModelVst3Paths>(model, std::move(paths));
  };
  spec.onPathsChanged = [&model](QObject* c, std::function<void(QStringList)> f) {
    QObject::connect(
        &model, &Media::Settings::Model::Vst3PathsChanged, c, std::move(f));
  };
  spec.rows = [&plug] {
    std::vector<Media::Settings::PluginTabRow> rows;
    rows.reserve(plug.vst_infos.size());
    for(const auto& p : plug.vst_infos)
      rows.push_back({p.name, p.path, p.isValid});
    return rows;
  };
  spec.onPluginsChanged = [&plug](QObject* c, std::function<void()> f) {
    QObject::connect(&plug, &vst3::ApplicationPlugin::vstChanged, c, std::move(f));
  };
  spec.rescan = [&plug] {
    plug.vst_infos.clear();
    plug.rescan();
  };

  return Media::Settings::makePluginSettingsWidget(std::move(spec));
}
}
