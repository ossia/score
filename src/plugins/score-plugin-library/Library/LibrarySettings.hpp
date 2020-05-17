#pragma once

#include <Process/TimeValue.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
#include <score/widgets/SpinBoxes.hpp>

#include <score_plugin_library_export.h>

#include <verdigris>

#define SETTINGS_UI_PATH_HPP(Control)                                 \
public:                                                               \
  void set##Control(QString);                                         \
  void Control##Changed(QString arg) W_SIGNAL(Control##Changed, arg); \
                                                                      \
private:                                                              \
  QLineEdit* m_##Control{};

#define SETTINGS_UI_PATH_SETUP(Text, Control)                   \
  m_##Control = new QLineEdit{m_widg};                          \
  lay->addRow(tr(Text), m_##Control);                           \
  connect(m_##Control, &QLineEdit::editingFinished, this, [=] { \
    Control##Changed(m_##Control->text());                      \
  });

#define SETTINGS_UI_PATH_IMPL(Control) \
  void View::set##Control(QString val) \
  {                                    \
    auto cur = m_##Control->text();    \
    if (cur != val)                    \
      m_##Control->setText(val);       \
  }

class QSpinBox;
class QCheckBox;
namespace Library::Settings
{
class SCORE_PLUGIN_LIBRARY_EXPORT Model final : public score::SettingsDelegateModel
{
  W_OBJECT(Model)
  QString m_Path;

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_LIBRARY_EXPORT, QString, Path)
};

SCORE_SETTINGS_PARAMETER(Model, Path)

class View : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();

  SETTINGS_UI_PATH_HPP(Path)

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};
};

class Presenter : public score::GlobalSettingsPresenter
{
public:
  using model_type = Model;
  using view_type = View;
  Presenter(Model&, View&, QObject* parent);

private:
  QString settingsName() override;
  QIcon settingsIcon() override;
};

SCORE_DECLARE_SETTINGS_FACTORY(
    Factory,
    Model,
    Presenter,
    View,
    "d6966670-f69f-48d0-96f6-72a5e2190cbc")
}
