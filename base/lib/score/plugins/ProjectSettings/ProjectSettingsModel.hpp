#pragma once
#include <QObject>
#include <score/command/SettingsCommand.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score_lib_base_export.h>
namespace score
{
class SCORE_LIB_BASE_EXPORT ProjectSettingsModel
    : public SerializableDocumentPlugin
{
public:
  using SerializableDocumentPlugin::SerializableDocumentPlugin;
  virtual ~ProjectSettingsModel();
};
}

#define SCORE_PROJECTSETTINGS_PARAMETER_TYPE(ModelType, Name)    \
  struct Name##Parameter                                         \
  {                                                              \
    using model_type = ModelType;                                \
    using param_type = decltype(ModelType().get##Name());        \
    static const constexpr auto getter = &model_type::get##Name; \
    static const constexpr auto setter = &model_type::set##Name; \
  };

#define SCORE_PROJECTSETTINGS_COMMAND(Name)                                \
  struct Set##Name : public score::ProjectSettingsCommand<Name##Parameter> \
  {                                                                        \
    SCORE_PROJECTSETTINGS_COMMAND_DECL(Set##Name)                          \
  };

#define SCORE_PROJECTSETTINGS_PARAMETER(ModelType, Name) \
  SCORE_PROJECTSETTINGS_PARAMETER_TYPE(ModelType, Name)  \
  SCORE_PROJECTSETTINGS_COMMAND(Name)
