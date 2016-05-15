#pragma once
#include <QObject>
#include <iscore_lib_base_export.h>
#include <iscore/command/SettingsCommand.hpp>

namespace iscore
{
    class SettingsDelegatePresenterInterface;
    class ISCORE_LIB_BASE_EXPORT SettingsDelegateModelInterface : public QObject
    {
        public:
            using QObject::QObject;
            virtual ~SettingsDelegateModelInterface();

        virtual void setFirstTimeSettings() = 0;
    };

}

#define ISCORE_SETTINGS_PARAMETER_TYPE(ModelType, Name) \
struct Name ## Parameter \
{ \
        using model_type = ModelType; \
        using param_type = decltype(ModelType().get ## Name()); \
        static const constexpr auto getter = &model_type::get ## Name; \
        static const constexpr auto setter = &model_type::set ## Name; \
};

#define ISCORE_SETTINGS_COMMAND(Name) \
    struct Set ## Name : public iscore::SettingsCommand<Name ## Parameter> \
{ \
 static constexpr const bool is_deferred = false; \
 ISCORE_SETTINGS_COMMAND_DECL(Set ## Name) \
};

#define ISCORE_SETTINGS_PARAMETER(ModelType, Name) \
    ISCORE_SETTINGS_PARAMETER_TYPE(ModelType, Name) \
    ISCORE_SETTINGS_COMMAND(Name)


#define ISCORE_SETTINGS_DEFERRED_COMMAND(Name) \
    struct Set ## Name : public iscore::SettingsCommand<Name ## Parameter> \
{ \
 static constexpr const bool is_deferred = true; \
 ISCORE_SETTINGS_COMMAND_DECL(Set ## Name) \
};

#define ISCORE_SETTINGS_DEFERRED_PARAMETER(ModelType, Name) \
    ISCORE_SETTINGS_PARAMETER_TYPE(ModelType, Name) \
    ISCORE_SETTINGS_DEFERRED_COMMAND(Name)
