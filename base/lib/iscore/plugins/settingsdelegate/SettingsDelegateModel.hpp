#pragma once
#include <QObject>
#include <iscore_lib_base_export.h>
#include <iscore/command/SettingsCommand.hpp>
#include <iscore/tools/Todo.hpp>

namespace iscore
{
    class ISCORE_LIB_BASE_EXPORT SettingsDelegateModel : public QObject
    {
        public:
            using QObject::QObject;
            virtual ~SettingsDelegateModel();

        virtual void setFirstTimeSettings() = 0;
    };

}

#define ISCORE_SETTINGS_COMMAND(ModelType, Name) \
    struct Set ## ModelType ## Name : public iscore::SettingsCommand<ModelType ## Name ## Parameter> \
{ \
 static constexpr const bool is_deferred = false; \
 ISCORE_SETTINGS_COMMAND_DECL(Set ## ModelType ## Name) \
};

#define ISCORE_SETTINGS_PARAMETER(ModelType, Name) \
    ISCORE_PARAMETER_TYPE(ModelType, Name) \
    ISCORE_SETTINGS_COMMAND(ModelType, Name)


#define ISCORE_SETTINGS_DEFERRED_COMMAND(ModelType, Name) \
    struct Set ## ModelType ## Name : public iscore::SettingsCommand<ModelType ## Name ## Parameter> \
{ \
 static constexpr const bool is_deferred = true; \
 ISCORE_SETTINGS_COMMAND_DECL(Set ## ModelType ## Name) \
};

#define ISCORE_SETTINGS_DEFERRED_PARAMETER(ModelType, Name) \
    ISCORE_PARAMETER_TYPE(ModelType, Name) \
    ISCORE_SETTINGS_DEFERRED_COMMAND(ModelType, Name)
