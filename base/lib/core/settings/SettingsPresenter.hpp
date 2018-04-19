#pragma once
#include <QObject>
#include <core/settings/SettingsView.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>
#include <vector>

namespace score
{
class SettingsDelegateModel;
template <class Model>
class SettingsDelegatePresenter;
using GlobalSettingsPresenter
    = SettingsDelegatePresenter<SettingsDelegateModel>;
} // namespace score

namespace score
{
class Settings;

template <class Model>
class SettingsPresenter final : public QObject
{
public:
  using Sv = SettingsView<Model>;
  SettingsPresenter(Sv* view, QObject* parent) : QObject{parent}, m_view{view}
  {
    connect(m_view, &Sv::accepted, this, &SettingsPresenter::on_accept);
    connect(m_view, &Sv::rejected, this, &SettingsPresenter::on_reject);
  }

  void addSettingsPresenter(SettingsDelegatePresenter<Model>* presenter)
  {
    SCORE_ASSERT(
        ossia::find(m_pluginPresenters, presenter)
        == m_pluginPresenters.end());

    m_pluginPresenters.push_back(presenter);
  }

private:
  void on_accept()
  {
    for (auto presenter : m_pluginPresenters)
    {
      presenter->on_accept();
    }
  }
  void on_reject()
  {
    for (auto presenter : m_pluginPresenters)
    {
      presenter->on_reject();
    }
  }
  Sv* m_view;

  std::vector<SettingsDelegatePresenter<Model>*> m_pluginPresenters;
};
}
