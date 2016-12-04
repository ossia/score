#pragma once
#include <QObject>
#include <vector>

namespace iscore
{
class SettingsDelegatePresenter;
} // namespace iscore

namespace iscore
{
class Settings;
class SettingsView;

class SettingsPresenter final : public QObject
{
  Q_OBJECT
public:
  SettingsPresenter(SettingsView* view, QObject* parent);

  void addSettingsPresenter(SettingsDelegatePresenter* presenter);

private:
  void on_accept();
  void on_reject();
  SettingsView* m_view;

  std::vector<SettingsDelegatePresenter*> m_pluginPresenters;
};
}
