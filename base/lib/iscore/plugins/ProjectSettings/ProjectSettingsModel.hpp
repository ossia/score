#pragma once
#include <QObject>
#include <iscore_lib_base_export.h>
#include <iscore/command/SettingsCommand.hpp>

namespace iscore
{
    class ProjectSettingsPresenter;
    class ISCORE_LIB_BASE_EXPORT ProjectSettingsModel : public QObject
    {
        public:
            using QObject::QObject;
            virtual ~ProjectSettingsModel();
    };

}

#define ISCORE_PROJECTSETTINGS_PARAMETER_TYPE(ModelType, Name) \
struct Name ## Parameter \
{ \
        using model_type = ModelType; \
        using param_type = decltype(ModelType().get ## Name()); \
        static const constexpr auto getter = &model_type::get ## Name; \
        static const constexpr auto setter = &model_type::set ## Name; \
};

#define ISCORE_PROJECTSETTINGS_COMMAND(Name) \
    struct Set ## Name : public iscore::ProjectSettingsCommand<Name ## Parameter> \
{ \
 ISCORE_PROJECTSETTINGS_COMMAND_DECL(Set ## Name) \
};

#define ISCORE_PROJECTSETTINGS_PARAMETER(ModelType, Name) \
    ISCORE_PROJECTSETTINGS_PARAMETER_TYPE(ModelType, Name) \
    ISCORE_PROJECTSETTINGS_COMMAND(Name)
