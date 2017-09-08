#pragma once
#include <QObject>
#include <core/settings/SettingsPresenter.hpp>
#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>
#include <score_lib_base_export.h>

namespace score
{
class ProjectSettingsModel;
class ProjectSettingsView;

class SCORE_LIB_BASE_EXPORT ProjectSettingsPresenter : public QObject
{
public:
  ProjectSettingsPresenter(
      ProjectSettingsModel& model, ProjectSettingsView& view, QObject* parent)
      : QObject{parent}, m_model{model}, m_view{view}
  {
  }

  virtual ~ProjectSettingsPresenter();
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
  ProjectSettingsModel& m_model;
  ProjectSettingsView& m_view;

  score::SettingsCommandDispatcher m_disp;
};
}
