// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/settings/Settings.hpp>
#include <core/settings/SettingsPresenter.hpp>
#include <core/settings/SettingsView.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsFactory.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsPresenter.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsView.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

namespace score
{
Settings::Settings()
{
}

Settings::~Settings()
{
  if (m_settingsView)
    m_settingsView->deleteLater();
  for (auto& ptr : m_settings)
  {
    auto p = ptr.release();
    p->deleteLater();
  }
}

void Settings::setupView()
{
  m_settingsView = new SettingsView<SettingsDelegateModel>(nullptr);
  m_settingsPresenter
      = new SettingsPresenter<SettingsDelegateModel>(m_settingsView, nullptr);
}

void Settings::setupSettingsPlugin(
    QSettings& s,
    const score::ApplicationContext& ctx,
    SettingsDelegateFactory& plugin)
{
  auto model = plugin.makeModel(s, ctx);
  if (!model)
    return;

  auto& model_ref = *model;
  m_settings.push_back(std::move(model));

  if (m_settingsView)
  {
    auto view = plugin.makeView();
    if (!view)
      return;

    auto pres = plugin.makePresenter(model_ref, *view, m_settingsPresenter);
    if (pres)
    {
      // Ownership transfer
      m_settingsPresenter->addSettingsPresenter(pres);
      m_settingsView->addSettingsView(view);
    }
    else
    {
      delete view;
    }
  }
}

ProjectSettings::ProjectSettings()
{
}

ProjectSettings::~ProjectSettings()
{
  if (m_settingsView)
    m_settingsView->deleteLater();
}

void ProjectSettings::setupView()
{
  m_settingsView = new SettingsView<ProjectSettingsModel>(nullptr);
  m_settingsPresenter
      = new SettingsPresenter<ProjectSettingsModel>(m_settingsView, nullptr);
}

void ProjectSettings::setup(const DocumentContext& ctx)
{
  m_settings.clear();
  setupView();

  for (auto& plug : ctx.document.model().pluginModels())
  {
    if (auto p = dynamic_cast<ProjectSettingsModel*>(plug))
    {
      m_settings.push_back(p);

      if (m_settingsView)
      {
        auto plug = ctx.app.interfaces<DocumentPluginFactoryList>().get(
            p->concreteKey().impl());
        if (!plug)
          continue;
        auto& plugin = *static_cast<ProjectSettingsFactory*>(plug);

        auto view = plugin.makeView();
        if (!view)
          return;

        auto pres = plugin.makePresenter(*p, *view, m_settingsPresenter);
        if (pres)
        {
          // Ownership transfer
          m_settingsPresenter->addSettingsPresenter(pres);
          m_settingsView->addSettingsView(view);
        }
        else
        {
          delete view;
        }
      }
    }
  }
}
}
