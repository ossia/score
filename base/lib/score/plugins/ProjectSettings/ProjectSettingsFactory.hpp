#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/documentdelegate/plugin/SerializableDocumentPlugin.hpp>
#include <score_lib_base_export.h>
namespace score
{
class DocumentPlugin;
template <class Model>
class SettingsDelegatePresenter;
template <class Model>
class SettingsDelegateView;

class ProjectSettingsModel;

using ProjectSettingsPresenter
    = SettingsDelegatePresenter<ProjectSettingsModel>;
using ProjectSettingsView = SettingsDelegateView<ProjectSettingsModel>;

/**
 * @brief The ProjectSettingsFactory class
 *
 * Reimplement in order to provide custom settings for the plug-in.
 */
class SCORE_LIB_BASE_EXPORT ProjectSettingsFactory
    : public DocumentPluginFactory
{
public:
  virtual ~ProjectSettingsFactory();

  virtual ProjectSettingsModel* makeModel(
      const score::DocumentContext&,
      Id<score::DocumentPlugin> id,
      QObject* parent)
      = 0;

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

template <typename Model_T, typename Presenter_T, typename View_T>
class ProjectSettingsDelegateFactory_T : public ProjectSettingsFactory
{
  ProjectSettingsModel* load(
      const VisitorVariant& var,
      score::DocumentContext& doc,
      QObject* parent) override
  {
    return deserialize_dyn(var, [&](auto&& deserializer) {
      return new Model_T{doc, deserializer, parent};
    });
  }

  ProjectSettingsModel* makeModel(
      const score::DocumentContext& ctx,
      Id<score::DocumentPlugin> id,
      QObject* parent) override
  {
    return new Model_T(ctx, id, parent);
  }

  score::ProjectSettingsView* makeView() override
  {
    return new View_T;
  }

  score::ProjectSettingsPresenter* makePresenter_impl(
      score::ProjectSettingsModel& m,
      score::ProjectSettingsView& v,
      QObject* parent) override
  {
    return new Presenter_T{safe_cast<Model_T&>(m), safe_cast<View_T&>(v),
                           parent};
  }
};
}

#define SCORE_DECLARE_PROJECTSETTINGS_FACTORY(          \
    Factory, Model, Presenter, View, Uuid)              \
  class Factory final                                   \
      : public score::ProjectSettingsDelegateFactory_T< \
            Model, Presenter, View>                     \
  {                                                     \
    SCORE_CONCRETE(Uuid)                                \
  };
