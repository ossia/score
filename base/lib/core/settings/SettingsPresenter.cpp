// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <core/settings/SettingsPresenter.hpp>
#include <core/settings/SettingsView.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>

namespace iscore
{
SettingsPresenter::SettingsPresenter(SettingsView* view, QObject* parent)
    : QObject{parent}, m_view{view}
{
  connect(
      m_view, &SettingsView::accepted, this, &SettingsPresenter::on_accept);
  connect(
      m_view, &SettingsView::rejected, this, &SettingsPresenter::on_reject);
}

void SettingsPresenter::addSettingsPresenter(
    SettingsDelegatePresenter* presenter)
{
  ISCORE_ASSERT(ossia::find(m_pluginPresenters, presenter) == m_pluginPresenters.end());

  m_pluginPresenters.push_back(presenter);
}

void SettingsPresenter::on_accept()
{
  for (auto presenter : m_pluginPresenters)
  {
    presenter->on_accept();
  }
}

void SettingsPresenter::on_reject()
{
  for (auto presenter : m_pluginPresenters)
  {
    presenter->on_reject();
  }
}
}
