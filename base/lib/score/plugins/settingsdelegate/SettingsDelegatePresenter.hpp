#pragma once
#include <QObject>
#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>
#include <score_lib_base_export.h>

namespace score
{
template <class Model>
class SettingsDelegateView;
class SettingsDelegateModel;

template <class Model>
class SettingsDelegatePresenter : public QObject
{
public:
  using SView = score::SettingsDelegateView<Model>;
  SettingsDelegatePresenter(Model& model, SView& view, QObject* parent)
      : QObject{parent}, m_model{model}, m_view{view}
  {
  }

  virtual ~SettingsDelegatePresenter() = default;
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
  Model& m_model;
  SView& m_view;

  score::SettingsCommandDispatcher m_disp;
};

using GlobalSettingsPresenter
    = SettingsDelegatePresenter<SettingsDelegateModel>;
using GlobalSettingsView = SettingsDelegateView<SettingsDelegateModel>;
}

#define SETTINGS_PRESENTER(Control)                                      \
  do                                                                     \
  {                                                                      \
    con(v, &View::Control##Changed, this, [&](auto val) {                \
      if (val != m.get##Control())                                       \
      {                                                                  \
        m_disp.submitCommand<SetModel##Control>(this->model(this), val); \
      }                                                                  \
    });                                                                  \
                                                                         \
    con(m, &Model::Control##Changed, &v, &View::set##Control);           \
    v.set##Control(m.get##Control());                                    \
  } while (0)
