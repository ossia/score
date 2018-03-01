#pragma once
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score_lib_base_export.h>
namespace score
{
template<class Model>
class SettingsDelegatePresenter;
template<class Model>
class SettingsDelegateView;

class ProjectSettingsModel;

using ProjectSettingsPresenter = SettingsDelegatePresenter<ProjectSettingsModel>;
using ProjectSettingsView = SettingsDelegateView<ProjectSettingsModel>;

/**
 * @brief The ProjectSettingsFactory class
 *
 * Reimplement in order to provide custom settings for the plug-in.
 */
class SCORE_LIB_BASE_EXPORT ProjectSettingsFactory
    : public score::Interface<ProjectSettingsFactory>
{
  SCORE_INTERFACE("18658b23-d20e-4a54-b16d-8f7072de9e9f")

public:
  virtual ~ProjectSettingsFactory();
  ProjectSettingsPresenter* makePresenter(
      score::ProjectSettingsModel& m,
      score::ProjectSettingsView& v,
      QObject* parent);
  virtual ProjectSettingsView* makeView() = 0;

protected:
  virtual ProjectSettingsPresenter* makePresenter_impl(
      score::ProjectSettingsModel& m,
      score::ProjectSettingsView& v,
      QObject* parent)
      = 0;
};

class SCORE_LIB_BASE_EXPORT ProjectSettingsFactoryList final
    : public InterfaceList<score::ProjectSettingsFactory>
{
public:
  using object_type = ProjectSettingsFactory;
};
}
