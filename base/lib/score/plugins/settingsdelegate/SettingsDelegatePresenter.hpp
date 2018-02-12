#pragma once
#include <QObject>
#include <core/settings/SettingsPresenter.hpp>
#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>
#include <score_lib_base_export.h>

namespace score
{
class SettingsDelegateModel;
class SettingsDelegateView;
class SettingsPresenter;

class SCORE_LIB_BASE_EXPORT SettingsDelegatePresenter : public QObject
{
public:
  SettingsDelegatePresenter(
      SettingsDelegateModel& model,
      SettingsDelegateView& view,
      QObject* parent)
      : QObject{parent}, m_model{model}, m_view{view}
  {
  }

  virtual ~SettingsDelegatePresenter();
  void on_accept()
  {
    m_disp.commit();
  }

  void on_reject()
  {
    m_disp.rollback();
  }

  virtual QString settingsName() = 0;
  virtual QIcon settingsIcon() = 0;

  template <typename T>
  auto& model(T* self)
  {
    return static_cast<typename T::model_type&>(self->m_model);
  }

  template <typename T>
  auto& view(T* self)
  {
    return static_cast<typename T::view_type&>(self->m_view);
  }

protected:
  SettingsDelegateModel& m_model;
  SettingsDelegateView& m_view;

  score::SettingsCommandDispatcher m_disp;
};
}

#define SETTINGS_PRESENTER(Control)                                         \
  do { con(v, &View:: Control ## Changed, this, [&](auto val) {             \
    if (val != m.get ## Control())                                          \
    {                                                                       \
      m_disp.submitCommand<SetModel ## Control>(this->model(this), val);    \
    }                                                                       \
  });                                                                       \
                                                                            \
  con(m, &Model::Control ## Changed, &v, &View::set ## Control);            \
  v.set ## Control(m.get ## Control()); } while(0)
